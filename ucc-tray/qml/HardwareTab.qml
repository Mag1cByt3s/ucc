import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*
 * Hardware tab — quick toggle controls for webcam, Fn Lock,
 * and display brightness.
 */
ScrollView {
    id: hwTab
    clip: true

    Flickable {
        contentHeight: col.implicitHeight + 24

        ColumnLayout {
            id: col
            width: hwTab.availableWidth
            spacing: 12

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 16
            }

            // ── Quick Toggles ──
            GroupBox {
                title: "Quick Controls"
                Layout.fillWidth: true

                RowLayout {
                    spacing: 24
                    width: parent.width

                    RowLayout {
                        spacing: 8
                        Label { text: "Fn Lock" }
                        Switch {
                            id: fnLockSwitch
                            checked: backend.fnLock
                            Connections {
                                target: backend
                                function onFnLockChanged() { fnLockSwitch.checked = backend.fnLock; }
                            }
                            onToggled: backend.setFnLock(checked)
                        }
                    }

                    RowLayout {
                        spacing: 8
                        Label { text: "Webcam" }
                        Switch {
                            id: webcamSwitch
                            checked: backend.webcamEnabled
                            Connections {
                                target: backend
                                function onWebcamEnabledChanged() { webcamSwitch.checked = backend.webcamEnabled; }
                            }
                            onToggled: backend.setWebcamEnabled(checked)
                        }
                    }
                }
            }

            // ── Display Brightness ──
            GroupBox {
                title: "Display Brightness"
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    width: parent.width

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true
                        Label { text: "\u2600"; font.pointSize: 12 }
                        Slider {
                            id: brightnessSlider
                            Layout.fillWidth: true
                            from: 0; to: 100; stepSize: 1
                            value: backend.displayBrightness
                            Connections {
                                target: backend
                                function onDisplayBrightnessChanged() { brightnessSlider.value = backend.displayBrightness; }
                            }
                            onPressedChanged: { if (!pressed) backend.setDisplayBrightness(Math.round(value)); }
                        }
                        Label {
                            text: Math.round(brightnessSlider.value) + "%"
                            Layout.preferredWidth: 40
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }


        }
    }
}

