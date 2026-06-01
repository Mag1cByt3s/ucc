# treefmt-nix configuration — one command to format Nix sources.
#
# Usage:   nix fmt                # format Nix files
#          nix flake check        # also verifies formatting
#
# Scope is intentionally limited to Nix today; C/C++ and other formatters can
# be added later once the project picks styles.
_: {
  projectRootFile = "flake.nix";

  settings.global.excludes = [
    "*.lock"
    "*.patch"
    "flake.lock"
    "build/**"
    "dist/**"
    "result"
    "result-*"
    "**/3rdparty/**"
  ];

  # Nix
  programs.nixfmt.enable = true;
  programs.statix.enable = true;
  programs.deadnix = {
    enable = true;
    no-lambda-pattern-names = true;
  };
}
