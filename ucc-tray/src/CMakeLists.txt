# SPDX-License-Identifier: GPL-3.0-or-later
#
# QML extension module: com.uniwill.ucc.private
# Provides the TrayBackend type to the Plasma applet.
#

ecm_add_qml_module( ucc-declarative URI com.uniwill.ucc.private )

set( ucc_SRCS
  TrayBackend.cpp TrayBackend.hpp
)

target_sources( ucc-declarative PRIVATE
  ${ucc_SRCS}
  plugin.cpp plugin.h
)

target_include_directories( ucc-declarative PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/../../libucc-dbus
)

target_link_libraries( ucc-declarative PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::DBus
  Qt6::Qml
  Qt6::Quick
)

# ── libucc-dbus linkage ──

if( NOT TARGET ucc-dbus AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../libucc-dbus/CMakeLists.txt )
  message( STATUS "Configuring local libucc-dbus subproject for ucc-tray build." )
  add_subdirectory(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../libucc-dbus
    ${CMAKE_CURRENT_BINARY_DIR}/libucc-dbus
  )
endif()

option( UCC_ENABLE_DEV_RPATH "Add build-tree RPATH for standalone run (dev only)" OFF )
if( TARGET ucc-dbus )
  add_dependencies( ucc-declarative ucc-dbus )
  if( UCC_ENABLE_DEV_RPATH )
    target_link_options( ucc-declarative PRIVATE "-Wl,-rpath,$<TARGET_FILE_DIR:ucc-dbus>" )
  endif()
  target_link_libraries( ucc-declarative PRIVATE $<TARGET_FILE:ucc-dbus> )
else()
  find_library( UCC_DBUS_LIB NAMES ucc-dbus
    PATHS
      ${CMAKE_CURRENT_SOURCE_DIR}/../../build/lib
      ${CMAKE_INSTALL_PREFIX}/lib
      /usr/lib
      /usr/lib64
    NO_DEFAULT_PATH
  )
  if( UCC_DBUS_LIB )
    target_link_libraries( ucc-declarative PRIVATE ${UCC_DBUS_LIB} )
  else()
    message( WARNING "Could not find libucc-dbus. Build may fail." )
  endif()
endif()

target_compile_options( ucc-declarative PRIVATE
  -Wall -Wextra -Wpedantic -Werror
)

ecm_finalize_qml_module( ucc-declarative DESTINATION ${KDE_INSTALL_QMLDIR} )

install( TARGETS ucc-declarative ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP )
