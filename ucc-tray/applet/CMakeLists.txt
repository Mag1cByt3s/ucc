# SPDX-License-Identifier: GPL-3.0-or-later

if("${CMAKE_LIBRARY_OUTPUT_DIRECTORY}" STREQUAL "")
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
endif()

plasma_add_applet( com.uniwill.ucc
  QML_SOURCES
    main.qml
    DashboardTab.qml
    PowerProfileTab.qml
    HardwareTab.qml
    WaterCoolerTab.qml
    KeyboardTab.qml
  GENERATE_APPLET_CLASS
)
