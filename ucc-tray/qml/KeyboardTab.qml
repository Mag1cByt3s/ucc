import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*
 * Keyboard tab — miniature keyboard visualiser (read-only)
 * with brightness slider control.
 *
 * Renders a simplified version of the full keyboard editor
 * using a grid where each cell is coloured by its zone's
 * current RGB state from the daemon.  No text labels — just
 * coloured key shapes matching the physical layout.
 */
ScrollView {
    id: kbTab
    clip: true

    // ── Zone-to-grid mapping ──
    // Each entry: { z: zoneId, r: row, c: col, w: colSpan, h: rowSpan }
    // Mirrors KeyboardVisualizerWidget.cpp layout
    readonly property var keyLayout: [
        // Row 0: Function keys
        { z: 105, r: 0, c: 0,  w: 1, h: 1 },
        { z: 106, r: 0, c: 1,  w: 1, h: 1 },
        { z: 107, r: 0, c: 2,  w: 1, h: 1 },
        { z: 108, r: 0, c: 3,  w: 1, h: 1 },
        { z: 109, r: 0, c: 4,  w: 1, h: 1 },
        { z: 110, r: 0, c: 5,  w: 1, h: 1 },
        { z: 111, r: 0, c: 6,  w: 1, h: 1 },
        { z: 112, r: 0, c: 7,  w: 1, h: 1 },
        { z: 113, r: 0, c: 8,  w: 1, h: 1 },
        { z: 114, r: 0, c: 9,  w: 1, h: 1 },
        { z: 115, r: 0, c: 10, w: 1, h: 1 },
        { z: 116, r: 0, c: 11, w: 1, h: 1 },
        { z: 117, r: 0, c: 12, w: 1, h: 1 },
        { z: 118, r: 0, c: 13, w: 1, h: 1 },
        { z: 119, r: 0, c: 14, w: 1, h: 1 },
        { z: 120, r: 0, c: 15, w: 1, h: 1 },
        { z: 121, r: 0, c: 16, w: 1, h: 1 },
        { z: 122, r: 0, c: 17, w: 1, h: 1 },
        { z: 123, r: 0, c: 18, w: 1, h: 1 },
        { z: 124, r: 0, c: 19, w: 1, h: 1 },
        // Row 1: Number row
        { z: 84,  r: 1, c: 0,  w: 1, h: 1 },
        { z: 85,  r: 1, c: 1,  w: 1, h: 1 },
        { z: 86,  r: 1, c: 2,  w: 1, h: 1 },
        { z: 87,  r: 1, c: 3,  w: 1, h: 1 },
        { z: 88,  r: 1, c: 4,  w: 1, h: 1 },
        { z: 89,  r: 1, c: 5,  w: 1, h: 1 },
        { z: 90,  r: 1, c: 6,  w: 1, h: 1 },
        { z: 91,  r: 1, c: 7,  w: 1, h: 1 },
        { z: 92,  r: 1, c: 8,  w: 1, h: 1 },
        { z: 93,  r: 1, c: 9,  w: 1, h: 1 },
        { z: 94,  r: 1, c: 10, w: 1, h: 1 },
        { z: 95,  r: 1, c: 11, w: 1, h: 1 },
        { z: 96,  r: 1, c: 12, w: 1, h: 1 },
        { z: 98,  r: 1, c: 13, w: 2, h: 1 },
        { z: 99,  r: 1, c: 15, w: 1, h: 1 },
        { z: 100, r: 1, c: 16, w: 1, h: 1 },
        { z: 101, r: 1, c: 17, w: 1, h: 1 },
        { z: 102, r: 1, c: 18, w: 1, h: 1 },
        // Row 2: QWERTY
        { z: 63,  r: 2, c: 0,  w: 2, h: 1 },
        { z: 65,  r: 2, c: 2,  w: 1, h: 1 },
        { z: 66,  r: 2, c: 3,  w: 1, h: 1 },
        { z: 67,  r: 2, c: 4,  w: 1, h: 1 },
        { z: 68,  r: 2, c: 5,  w: 1, h: 1 },
        { z: 69,  r: 2, c: 6,  w: 1, h: 1 },
        { z: 70,  r: 2, c: 7,  w: 1, h: 1 },
        { z: 71,  r: 2, c: 8,  w: 1, h: 1 },
        { z: 72,  r: 2, c: 9,  w: 1, h: 1 },
        { z: 73,  r: 2, c: 10, w: 1, h: 1 },
        { z: 74,  r: 2, c: 11, w: 1, h: 1 },
        { z: 75,  r: 2, c: 12, w: 1, h: 1 },
        { z: 76,  r: 2, c: 13, w: 1, h: 1 },
        { z: 77,  r: 2, c: 14, w: 1, h: 2 },
        { z: 36,  r: 2, c: 16, w: 1, h: 1 },
        { z: 37,  r: 2, c: 17, w: 1, h: 1 },
        { z: 38,  r: 2, c: 18, w: 1, h: 1 },
        { z: 39,  r: 2, c: 19, w: 1, h: 2 },
        // Row 3: Home row
        { z: 42,  r: 3, c: 0,  w: 2, h: 1 },
        { z: 44,  r: 3, c: 2,  w: 1, h: 1 },
        { z: 45,  r: 3, c: 3,  w: 1, h: 1 },
        { z: 46,  r: 3, c: 4,  w: 1, h: 1 },
        { z: 47,  r: 3, c: 5,  w: 1, h: 1 },
        { z: 48,  r: 3, c: 6,  w: 1, h: 1 },
        { z: 49,  r: 3, c: 7,  w: 1, h: 1 },
        { z: 50,  r: 3, c: 8,  w: 1, h: 1 },
        { z: 51,  r: 3, c: 9,  w: 1, h: 1 },
        { z: 52,  r: 3, c: 10, w: 1, h: 1 },
        { z: 53,  r: 3, c: 11, w: 1, h: 1 },
        { z: 54,  r: 3, c: 12, w: 1, h: 1 },
        { z: 55,  r: 3, c: 13, w: 1, h: 1 },
        { z: 57,  r: 3, c: 16, w: 1, h: 1 },
        { z: 58,  r: 3, c: 17, w: 1, h: 1 },
        { z: 59,  r: 3, c: 18, w: 1, h: 1 },
        // Row 4: Shift row
        { z: 22,  r: 4, c: 0,  w: 1, h: 1 },
        { z: 23,  r: 4, c: 1,  w: 1, h: 1 },
        { z: 24,  r: 4, c: 2,  w: 1, h: 1 },
        { z: 25,  r: 4, c: 3,  w: 1, h: 1 },
        { z: 26,  r: 4, c: 4,  w: 1, h: 1 },
        { z: 27,  r: 4, c: 5,  w: 1, h: 1 },
        { z: 28,  r: 4, c: 6,  w: 1, h: 1 },
        { z: 29,  r: 4, c: 7,  w: 1, h: 1 },
        { z: 30,  r: 4, c: 8,  w: 1, h: 1 },
        { z: 31,  r: 4, c: 9,  w: 1, h: 1 },
        { z: 32,  r: 4, c: 10, w: 1, h: 1 },
        { z: 33,  r: 4, c: 11, w: 1, h: 1 },
        { z: 35,  r: 4, c: 12, w: 3, h: 1 },
        { z: 14,  r: 4, c: 15, w: 1, h: 1 },
        { z: 78,  r: 4, c: 16, w: 1, h: 1 },
        { z: 79,  r: 4, c: 17, w: 1, h: 1 },
        { z: 80,  r: 4, c: 18, w: 1, h: 1 },
        { z: 81,  r: 4, c: 19, w: 1, h: 2 },
        // Row 5: Bottom row
        { z: 0,   r: 5, c: 0,  w: 1, h: 1 },
        { z: 2,   r: 5, c: 1,  w: 1, h: 1 },
        { z: 3,   r: 5, c: 2,  w: 1, h: 1 },
        { z: 4,   r: 5, c: 3,  w: 1, h: 1 },
        { z: 7,   r: 5, c: 4,  w: 5, h: 1 },
        { z: 10,  r: 5, c: 9,  w: 1, h: 1 },
        { z: 12,  r: 5, c: 10, w: 1, h: 1 },
        { z: 13,  r: 5, c: 14, w: 1, h: 1 },
        { z: 18,  r: 5, c: 15, w: 1, h: 1 },
        { z: 15,  r: 5, c: 16, w: 1, h: 1 },
        { z: 16,  r: 5, c: 17, w: 2, h: 1 },
        { z: 17,  r: 5, c: 19, w: 1, h: 1 }
    ]

    function getZoneColor(zoneId) {
        var states = backend.keyboardBacklightStates;
        var info = backend.keyboardBacklightInfo;
        var maxBr = (info && info.maxBrightness !== undefined) ? info.maxBrightness : 255;
        var zones = backend.keyboardZoneCount;

        if (zones <= 0)
            return "#404040";

        // Per-key RGB (126 zones) — direct index
        if (zones > 3 && zoneId < states.length) {
            var s = states[zoneId];
            if (s) {
                var br = (s.brightness !== undefined) ? s.brightness / Math.max(1, maxBr) : 1.0;
                return Qt.rgba(
                    (s.red || 0) / 255.0 * br,
                    (s.green || 0) / 255.0 * br,
                    (s.blue || 0) / 255.0 * br,
                    1.0
                );
            }
        }

        // 1–3 zone keyboards — map keys to zone by column region
        if (zones >= 1 && states.length > 0) {
            var keyCol = 10;
            for (var i = 0; i < keyLayout.length; i++) {
                if (keyLayout[i].z === zoneId) {
                    keyCol = keyLayout[i].c;
                    break;
                }
            }
            var zoneIdx = Math.min(zones - 1, Math.floor(keyCol / (20 / zones)));
            if (zoneIdx < states.length) {
                var sz = states[zoneIdx];
                var brz = (sz.brightness !== undefined) ? sz.brightness / Math.max(1, maxBr) : 1.0;
                return Qt.rgba(
                    (sz.red || 0) / 255.0 * brz,
                    (sz.green || 0) / 255.0 * brz,
                    (sz.blue || 0) / 255.0 * brz,
                    1.0
                );
            }
        }

        return "#404040";
    }

    Flickable {
        contentHeight: col.implicitHeight + 24

        ColumnLayout {
            id: col
            width: kbTab.availableWidth
            spacing: 12

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 12
            }

            // ── Miniature Keyboard Visualiser ──
            GroupBox {
                title: "Keyboard Backlight"
                Layout.fillWidth: true

                Rectangle {
                    id: kbContainer
                    width: parent.width
                    height: cellSize * 6 + 5 * cellGap + 16
                    radius: 6
                    color: "#1a1a1a"
                    border.color: palette.mid

                    property real cellSize: Math.max(12, Math.min(22, (width - 19 * cellGap - 16) / 20))
                    property real cellGap: 2

                    Repeater {
                        model: backend.keyboardZoneCount > 0 ? keyLayout : []

                        delegate: Rectangle {
                            required property var modelData
                            x: 8 + modelData.c * (kbContainer.cellSize + kbContainer.cellGap)
                            y: 8 + modelData.r * (kbContainer.cellSize + kbContainer.cellGap)
                            width: modelData.w * kbContainer.cellSize + (modelData.w - 1) * kbContainer.cellGap
                            height: modelData.h * kbContainer.cellSize + (modelData.h - 1) * kbContainer.cellGap
                            radius: 2
                            color: getZoneColor(modelData.z)
                            border.color: Qt.lighter(color, 1.3)
                            border.width: 0.5
                        }
                    }

                    Label {
                        anchors.centerIn: parent
                        text: "No keyboard backlight detected"
                        color: "#888"
                        font.pointSize: 9
                        visible: backend.keyboardZoneCount <= 0
                    }
                }
            }

            // ── Brightness Slider ──
            GroupBox {
                title: "Backlight Brightness"
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    width: parent.width

                    RowLayout {
                        spacing: 12
                        Layout.fillWidth: true

                        Label { text: "\u2600" ; font.pointSize: 10 }

                        Slider {
                            id: kbBrightnessSlider
                            Layout.fillWidth: true
                            from: 0
                            to: {
                                var info = backend.keyboardBacklightInfo;
                                if (info && info.maxBrightness !== undefined)
                                    return info.maxBrightness;
                                return 255;
                            }
                            stepSize: 1
                            value: {
                                var info = backend.keyboardBacklightInfo;
                                if (info && info.brightness !== undefined)
                                    return info.brightness;
                                return 128;
                            }

                            onPressedChanged: {
                                if (!pressed) {
                                    backend.setKeyboardBrightness(Math.round(value));
                                }
                            }
                        }

                        Label {
                            text: Math.round(kbBrightnessSlider.value)
                            Layout.preferredWidth: 36
                            horizontalAlignment: Text.AlignRight
                        }
                    }
                }
            }

            // ── Info Note ──
            Label {
                text: "Per-key colour editing is available in the full Control Center."
                font.pointSize: 9
                opacity: 0.6
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
    }
}
