{ overlay ? null }:
{ config, lib, pkgs, ... }:

let
  cfg = config.services.uccd;
  extraArgsString =
    lib.concatStringsSep " " (map lib.escapeShellArg cfg.extraArgs);
  videoDrivers = config.services.xserver.videoDrivers or [ ];
  nvidiaPackage = lib.attrByPath [ "hardware" "nvidia" "package" ] null config;
  hasNvidia = nvidiaPackage != null && lib.elem "nvidia" videoDrivers;
  nvidiaBinPath =
    lib.optionalString hasNvidia
      ":${lib.makeBinPath [ nvidiaPackage ]}";
  # libnvidia-ml.so.1 is loaded via dlopen() at runtime; NixOS exposes it under
  # /run/opengl-driver/lib which is always set up when nvidia drivers are enabled.
  nvidiaLibPath =
    lib.optionalString hasNvidia
      "/run/opengl-driver/lib";
  uccdToolsPath = lib.makeBinPath [
    pkgs.coreutils
    pkgs.gawk
    pkgs.gnugrep
    pkgs.procps
    pkgs.util-linux
    pkgs.which
  ];
in
{
  options.services.uccd = {
    enable = lib.mkEnableOption "Uniwill Control Center daemon (uccd)";

    package = lib.mkOption {
      type = lib.types.package;
      default =
        if pkgs ? ucc then
          pkgs.ucc
        else
          pkgs.callPackage ../package.nix { src = ../.; };
      defaultText = "pkgs.ucc (or callPackage ../package.nix)";
      description = "The `ucc` package providing `uccd`.";
    };

    extraArgs = lib.mkOption {
      type = lib.types.listOf lib.types.str;
      default = [ ];
      example = [ "--verbose" ];
      description = "Extra arguments passed to `uccd --start`.";
    };

    enableSleepHandler = lib.mkOption {
      type = lib.types.bool;
      default = true;
      description = "Restart `uccd` on suspend/hibernate via a small systemd helper unit.";
    };
  };

  config = lib.mkIf cfg.enable {
    nixpkgs.overlays = lib.mkIf (overlay != null) [ overlay ];

    # Add the package to the system profile so plasmashell can discover the
    # Plasma applet .so (via QT_PLUGIN_PATH) and the QML module
    # (com.uniwill.ucc.private, via QML2_IMPORT_PATH / QT_QML_IMPORT_PATH).
    environment.systemPackages = [ cfg.package ];

    # Install the Polkit policy so that unprivileged GUI users can
    # authenticate for privileged D-Bus operations.
    security.polkit.enable = true;

    # Make the D-Bus service config and Polkit policy discoverable at the
    # system level.  environment.systemPackages alone is not sufficient for
    # system-bus configs and Polkit action definitions.
    services.dbus.packages = [ cfg.package ];
    environment.pathsToLink = [ "/share/polkit-1" ];

    systemd.tmpfiles.rules = [
      "d /etc/ucc 0755 root root - -"
    ];

    systemd.services.uccd = {
      description = "Uniwill Control Center Daemon";
      documentation = [ "man:uccd(8)" ];
      after = [ "dbus.service" ];
      requires = [ "dbus.service" ];
      wantedBy = [ "multi-user.target" ];

      serviceConfig = {
        Type = "dbus";
        BusName = "com.uniwill.uccd";
        ExecStartPre = "-${cfg.package}/bin/uccd --stop";
        ExecStart =
          "${cfg.package}/bin/uccd --start"
          + lib.optionalString (cfg.extraArgs != [ ]) " ${extraArgsString}";
        Environment = [
          "PATH=/run/wrappers/bin:/run/current-system/sw/bin:${uccdToolsPath}${nvidiaBinPath}"
        ] ++ lib.optionals hasNvidia [
          "LD_LIBRARY_PATH=${nvidiaLibPath}"
        ];
        Restart = "on-failure";
        RestartSec = "5s";
        TimeoutStopSec = "10s";
        User = "root";
        StandardOutput = "journal";
        StandardError = "journal";
        SyslogIdentifier = "uccd";

        PrivateTmp = true;
        ProtectSystem = "strict";
        ProtectHome = true;
        NoNewPrivileges = true;
        ReadWritePaths = [ "/etc/ucc" "/run" ];
      };
    };

    # Stop UCCD *before* the system enters sleep so that NVML is cleanly
    # shut down while the NVIDIA GPU is still fully operational.  Without
    # this, the NVML/CUDA cleanup runs *after* a suspend-resume cycle when
    # the nvidia_uvm driver state is stale, causing the CUDA teardown
    # thread to deadlock in uvm_va_space_mm_shutdown.  That stuck thread
    # (in uninterruptible "D" state) survives SIGKILL, blocks the kernel
    # process freezer on the next suspend attempt, and can cascade into
    # GSP heartbeat timeouts, KWin DRM-master loss, and an unresponsive
    # desktop.
    #
    # Ordering: uccd-pre-sleep → nvidia-suspend → systemd-suspend
    systemd.services.uccd-pre-sleep = lib.mkIf cfg.enableSleepHandler {
      description = "Stop UCCD before system sleep";
      documentation = [ "man:uccd(8)" ];
      before = [
        "systemd-suspend.service"
        "systemd-hibernate.service"
        "systemd-hybrid-sleep.service"
        "systemd-suspend-then-hibernate.service"
        "nvidia-suspend.service"
        "nvidia-hibernate.service"
      ];
      wantedBy = [
        "suspend.target"
        "hibernate.target"
        "hybrid-sleep.target"
        "suspend-then-hibernate.target"
      ];

      serviceConfig = {
        Type = "oneshot";
        ExecStart = "${config.systemd.package}/bin/systemctl stop uccd.service";
      };
    };

    # Start UCCD again after the system resumes.  At this point the NVIDIA
    # driver has already restored GPU state (nvidia-resume.service), so
    # UCCD initialises NVML against a clean, fully operational GPU.
    systemd.services.uccd-sleep = lib.mkIf cfg.enableSleepHandler {
      description = "Start UCCD after system resume";
      documentation = [ "man:uccd(8)" ];
      after = [
        "suspend.target"
        "hibernate.target"
        "hybrid-sleep.target"
        "suspend-then-hibernate.target"
      ];
      wantedBy = [
        "suspend.target"
        "hibernate.target"
        "hybrid-sleep.target"
        "suspend-then-hibernate.target"
      ];

      serviceConfig = {
        Type = "oneshot";
        ExecStart = "${config.systemd.package}/bin/systemctl start uccd.service";
      };
    };
  };
}
