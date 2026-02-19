import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*
 * Dashboard tab — live system monitoring overview.
 *
 * Displays CPU/GPU temperatures, frequencies, power draw and fan speeds.
 * Water cooler status is shown on the Water Cooler tab.
 */
ScrollView {
    id: dashTab
    clip: true

    Flickable {
        contentHeight: col.implicitHeight + 24

        ColumnLayout {
            id: col
            width: dashTab.availableWidth
            spacing: 12

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 16
            }

            // ── Active Profile ──
            GroupBox {
                title: "Active Profile"
                Layout.fillWidth: true

                RowLayout {
                    spacing: 12
                    width: parent.width

                    Label {
                        text: backend.activeProfileName || "—"
                        font.pointSize: 13
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Label {
                        text: backend.powerState || ""
                        font.pointSize: 10
                        opacity: 0.7
                    }
                }
            }

            // ── CPU Section ──
            GroupBox {
                title: "CPU"
                Layout.fillWidth: true

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 6
                    width: parent.width

                    Label { text: "Temperature"; opacity: 0.7 }
                    Label { text: backend.cpuTemp + " \u00b0C"; font.bold: true }
                    Label { text: "Frequency"; opacity: 0.7 }
                    Label { text: backend.cpuFreqMHz + " MHz"; font.bold: true }

                    Label { text: "Power"; opacity: 0.7 }
                    Label { text: backend.cpuPowerW.toFixed(1) + " W"; font.bold: true }
                    Label { text: "Fan"; opacity: 0.7 }
                    Label { text: backend.cpuFanRPM + " RPM (" + backend.cpuFanPercent + "%)"; font.bold: true }
                }
            }

            // ── GPU Section ──
            GroupBox {
                title: "GPU"
                Layout.fillWidth: true

                GridLayout {
                    columns: 4
                    columnSpacing: 16
                    rowSpacing: 6
                    width: parent.width

                    Label { text: "Temperature"; opacity: 0.7 }
                    Label { text: backend.gpuTemp + " \u00b0C"; font.bold: true }
                    Label { text: "Frequency"; opacity: 0.7 }
                    Label { text: backend.gpuFreqMHz + " MHz"; font.bold: true }

                    Label { text: "Power"; opacity: 0.7 }
                    Label { text: backend.gpuPowerW.toFixed(1) + " W"; font.bold: true }
                    Label { text: "Fan"; opacity: 0.7 }
                    Label { text: backend.gpuFanRPM + " RPM (" + backend.gpuFanPercent + "%)"; font.bold: true }
                }
            }
        }
    }
}
