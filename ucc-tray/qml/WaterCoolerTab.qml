import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

/*
 * Water Cooler tab — live status plus fan speed, pump voltage,
 * and LED colour/mode controls wired to TrayBackend.
 */
ScrollView {
    id: wcTab
    clip: true

    // PumpVoltage enum values indexed by combo position:
    //   0→Off(4)  1→7V(2)  2→8V(3)  3→11V(0)
    readonly property var pumpVoltageCodes: [4, 2, 3, 0]

    // Human-readable label for a raw PumpVoltage code (V11=0,V12=1,V7=2,V8=3,Off=4)
    readonly property var pumpVoltageLabels: ["High", "Max", "Low", "Medium", "Off"]

    function pumpLevelText(code) {
        if (code >= 0 && code < pumpVoltageLabels.length) return pumpVoltageLabels[code]
        return "—"
    }

    function voltageCodeToIndex(code) {
        for (var i = 0; i < pumpVoltageCodes.length; i++)
            if (pumpVoltageCodes[i] === code) return i
        return 0
    }

    Flickable {
        contentHeight: col.implicitHeight + 24

        ColumnLayout {
            id: col
            width: wcTab.availableWidth
            spacing: 12

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 16
            }

            // ── Status ──
            GroupBox {
                title: "Water Cooler Status"
                Layout.fillWidth: true

                RowLayout {
                    spacing: 20
                    width: parent.width

                    RowLayout {
                        spacing: 4
                        Label { text: "Enable" }
                        CheckBox {
                            id: wcEnableCheckBox
                            checked: backend.wcEnabled
                            Connections {
                                target: backend
                                function onWcEnabledChanged() { wcEnableCheckBox.checked = backend.wcEnabled }
                            }
                            onToggled: backend.setWcEnabled(checked)
                        }
                    }

                    GridLayout {
                        columns: 4
                        columnSpacing: 16
                        rowSpacing: 8
                        Layout.fillWidth: true

                        Label { text: "Fan Duty Cycle"; opacity: 0.7 }
                        Label {
                            text: backend.wcFanSpeed > 0 ? (backend.wcFanSpeed + "%") : "—"
                            font.bold: true
                        }

                        Label { text: "Pump Level"; opacity: 0.7 }
                        Label {
                            text: pumpLevelText(backend.wcPumpLevel)
                            font.bold: true
                        }
                    }
                }
            }

            // ── Fan Speed & Pump Voltage Controls (side by side) ──
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                // ── Fan Speed Control ──
                GroupBox {
                    title: "Fan Speed"
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 8
                        width: parent.width

                        RowLayout {
                            spacing: 8
                            Layout.fillWidth: true

                            Slider {
                                id: wcFanSlider
                                Layout.fillWidth: true
                                from: 0
                                to: 100
                                stepSize: 5
                                value: backend.wcFanPercent
                                enabled: backend.wcConnected && !backend.wcAutoControl

                                Connections {
                                    target: backend
                                    function onWcControlStateChanged() {
                                        if (!wcFanSlider.pressed)
                                            wcFanSlider.value = backend.wcFanPercent
                                    }
                                }

                                onPressedChanged: {
                                    if (!pressed)
                                        backend.setWcFanSpeed(Math.round(value))
                                }
                            }

                            Label {
                                text: Math.round(wcFanSlider.value) + "%"
                                Layout.preferredWidth: 36
                                horizontalAlignment: Text.AlignRight
                            }
                        }
                    }
                }

                // ── Pump Voltage ──
                GroupBox {
                    title: "Pump Voltage"
                    Layout.fillWidth: true

                    ColumnLayout {
                        spacing: 8
                        width: parent.width

                        ComboBox {
                            id: pumpCombo
                            Layout.fillWidth: true
                            model: ["Off", "7 V", "8 V", "11 V"]
                            enabled: backend.wcConnected && !backend.wcAutoControl
                            currentIndex: wcTab.voltageCodeToIndex(backend.wcPumpVoltageCode)

                            Connections {
                                target: backend
                                function onWcControlStateChanged() {
                                    pumpCombo.currentIndex = wcTab.voltageCodeToIndex(backend.wcPumpVoltageCode)
                                }
                            }

                            onActivated: backend.setWcPumpVoltageCode(wcTab.pumpVoltageCodes[currentIndex])
                        }
                    }
                }
            }

            // ── LED Control ──
            GroupBox {
                title: "LED"
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    width: parent.width

                    // LED enable + colour + mode on one row
                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        // LED checkbox — label left, box right (mirrors GUI's RightToLeft)
                        Label { text: "LED" }
                        CheckBox {
                            id: ledCheckBox
                            checked: backend.wcLedEnabled
                            enabled: backend.wcConnected
                            topPadding: 0; bottomPadding: 0

                            Connections {
                                target: backend
                                function onWcControlStateChanged() {
                                    ledCheckBox.checked = backend.wcLedEnabled
                                }
                            }
                            onToggled: backend.setWcLedEnabled(checked)
                        }

                        // Plain "Choose Color" button — enabled only for Static mode with LED on
                        Button {
                            text: "Choose Color"
                            enabled: backend.wcConnected &&
                                     ledCheckBox.checked &&
                                     ledModeCombo.currentIndex === 0
                            onClicked: ledColorDialog.open()
                        }

                        ColorDialog {
                            id: ledColorDialog
                            title: "Choose LED Colour"
                            selectedColor: Qt.rgba(backend.wcLedRed / 255,
                                                   backend.wcLedGreen / 255,
                                                   backend.wcLedBlue / 255, 1.0)
                            onAccepted: backend.setWcLed(
                                Math.round(selectedColor.r * 255),
                                Math.round(selectedColor.g * 255),
                                Math.round(selectedColor.b * 255),
                                ledModeCombo.currentIndex)
                        }

                        Label { text: "Mode:" }

                        ComboBox {
                            id: ledModeCombo
                            Layout.fillWidth: true
                            model: ["Static", "Breathe", "Colourful", "Breathe Colour", "Temperature"]
                            enabled: backend.wcConnected && ledCheckBox.checked
                            currentIndex: backend.wcLedMode

                            Connections {
                                target: backend
                                function onWcControlStateChanged() {
                                    ledModeCombo.currentIndex = backend.wcLedMode
                                }
                            }

                            onActivated: backend.setWcLed(
                                backend.wcLedRed,
                                backend.wcLedGreen,
                                backend.wcLedBlue,
                                currentIndex)
                        }
                    }
                }
            }
        }
    }
}
