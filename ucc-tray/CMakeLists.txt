# SPDX-License-Identifier: GPL-3.0-or-later
#
# ucc-tray — KDE Plasma system-tray applet for Uniwill Control Center
#
# Structure (following plasma-pa pattern):
#   src/     — QML extension module (com.uniwill.ucc.private) providing TrayBackend
#   applet/  — Plasma applet package (com.uniwill.ucc) with the QML UI
#

# This directory previously hosted a standalone QtQuick tray executable
# (`main.cpp`, resources, desktop files). The tray is now a Plasma applet in
# `applet/` with a QML extension module in `src/`.
#
# To keep local/packaging builds working even if someone reintroduces legacy
# sources, choose what to build based on which sources exist.

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  cmake_minimum_required(VERSION 3.20)

  project(
    ucc-tray
    VERSION 0.1.0
    DESCRIPTION "Uniwill Control Center tray"
    LANGUAGES CXX
  )

  set(CMAKE_CXX_STANDARD 20)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  set(CMAKE_AUTOMOC ON)
  set(CMAKE_AUTORCC ON)

  find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Qml Quick QuickControls2)

  find_package(ECM REQUIRED NO_MODULE)
  set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
  include(KDEInstallDirs)
  include(KDECMakeSettings)
  include(ECMQmlModule)

  find_package(KF6Config REQUIRED)
  find_package(KF6CoreAddons REQUIRED)
  find_package(Plasma REQUIRED)
endif()

set(_ucc_tray_has_plasma_applet FALSE)
if(
  EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt"
  AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/applet/CMakeLists.txt"
)
  set(_ucc_tray_has_plasma_applet TRUE)
endif()

if(_ucc_tray_has_plasma_applet)
  add_subdirectory(src)
  add_subdirectory(applet)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
  message(STATUS "ucc-tray: building legacy tray executable")

  # Optional: LayerShellQt for Wayland popup positioning.
  find_package(LayerShellQt QUIET)
  if(LayerShellQt_FOUND)
    message(STATUS "LayerShellQt found — Wayland layer-shell positioning enabled")
  endif()

  add_executable(
    ucc-tray
    main.cpp
    TrayBackend.cpp
    tray-resources.qrc
  )

  target_include_directories(
    ucc-tray
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../libucc-dbus
  )

  # Allow building the tray standalone: if the lib source is present and the
  # target doesn't already exist, add the libucc-dbus subproject so the ucc-dbus
  # target becomes available here.
  if(NOT TARGET ucc-dbus AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../libucc-dbus/CMakeLists.txt)
    message(STATUS "Configuring local libucc-dbus subproject for standalone tray build.")
    add_subdirectory(
      ${CMAKE_CURRENT_SOURCE_DIR}/../libucc-dbus
      ${CMAKE_CURRENT_BINARY_DIR}/libucc-dbus
    )
  endif()

  target_link_libraries(
    ucc-tray
    PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
  )

  if(LayerShellQt_FOUND)
    target_compile_definitions(ucc-tray PRIVATE UCC_HAVE_LAYER_SHELL_QT)
    target_link_libraries(ucc-tray PRIVATE LayerShellQt::Interface)
  endif()

  option(UCC_ENABLE_DEV_RPATH "Add build-tree RPATH for standalone run (development only)" OFF)
  if(TARGET ucc-dbus)
    if(UCC_ENABLE_DEV_RPATH)
      target_link_options(ucc-tray PRIVATE "-Wl,-rpath,$<TARGET_FILE_DIR:ucc-dbus>")
    endif()
    target_link_libraries(ucc-tray PRIVATE $<TARGET_FILE:ucc-dbus>)
    add_dependencies(ucc-tray ucc-dbus)
  endif()

  install(TARGETS ucc-tray RUNTIME DESTINATION bin)
else()
  message(
    FATAL_ERROR
    "ucc-tray: no buildable sources found (expected src/ + applet/ for Plasma applet, or legacy main.cpp)."
  )
endif()
