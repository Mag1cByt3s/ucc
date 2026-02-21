# SPDX-License-Identifier: GPL-3.0-or-later
#
# ucc-tray — KDE Plasma system-tray applet for Uniwill Control Center
#
# Structure (following plasma-pa pattern):
#   src/     — QML extension module (com.uniwill.ucc.private) providing TrayBackend
#   applet/  — Plasma applet package (com.uniwill.ucc) with the QML UI
#

add_subdirectory( src )
add_subdirectory( applet )
