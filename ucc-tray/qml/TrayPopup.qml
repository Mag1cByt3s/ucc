import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*
 * Root popup window for the UCC system-tray applet.
 *
 * Provides a tabbed interface with:
 *   Dashboard | Profile | Water Cooler | Hardware
 *
 * The "backend" context property is a TrayBackend instance set from C++.
 */
Rectangle {
    id: root
    color: palette.window
    width: 520
    height: 400

    // Use system palette so we follow the desktop theme automatically
    SystemPalette { id: palette; colorGroup: SystemPalette.Active }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // ── Header ──
        Rectangle {
            Layout.fillWidth: true
            height: 40
            color: Qt.darker(palette.window, 1.08)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 8
                spacing: 8

                Label {
                    text: "Uniwill Control Center"
                    font.bold: true
                    font.pointSize: 11
                    color: palette.windowText
                    Layout.fillWidth: true
                }

                ToolButton {
                    text: "\u2197"  // open-in-new icon
                    font.pointSize: 14
                    ToolTip.text: "Open full Control Center"
                    ToolTip.visible: hovered
                    onClicked: backend.openControlCenter()
                }

                ToolButton {
                    text: "\u21BB"  // refresh
                    font.pointSize: 14
                    ToolTip.text: "Refresh"
                    ToolTip.visible: hovered
                    onClicked: backend.refreshAll()
                }
            }
        }

        // ── Tab Bar ──
        TabBar {
            id: tabBar
            Layout.fillWidth: true

            TabButton { text: "Dashboard" }
            TabButton { text: "Profile" }
            TabButton {
                text: "Water Cooler"
                visible: backend.waterCoolerSupported
            }
            TabButton { text: "Hardware" }
        }

        // ── Tab Content ──
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabBar.currentIndex

            DashboardTab {}
            PowerProfileTab {}
            WaterCoolerTab {}
            HardwareTab {}
        }

        // ── Footer ──
        Rectangle {
            Layout.fillWidth: true
            height: 28
            color: Qt.darker(palette.window, 1.08)

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12

                Label {
                    text: backend.connected ? "\u2713 Connected" : "\u2717 Disconnected"
                    font.pointSize: 9
                    color: backend.connected ? "#4caf50" : "#f44336"
                }

                Item { Layout.fillWidth: true }

                Label {
                    text: backend.powerState ? ("Power: " + backend.powerState) : ""
                    font.pointSize: 9
                    color: palette.windowText
                    opacity: 0.7
                }
            }
        }
    }
}
