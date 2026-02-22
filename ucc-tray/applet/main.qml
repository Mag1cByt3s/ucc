/*
 * SPDX-FileCopyrightText: 2026 Uniwill Control Center Contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

import QtQuick
import QtQuick.Layouts

import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.plasmoid
import org.kde.plasma.components as PC3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.kirigami as Kirigami

PlasmoidItem {
    id: main

    TrayBackend {
        id: trayBackend
    }

    switchWidth: Kirigami.Units.gridUnit * 20
    switchHeight: Kirigami.Units.gridUnit * 14

    Plasmoid.icon: "ucc-tray"

    toolTipMainText: "Uniwill Control Center"
    toolTipSubText: {
        if (!trayBackend.connected)
            return i18n("Disconnected from uccd")
        var parts = []
        if (trayBackend.cpuTemp > 0)
            parts.push(i18n("CPU: %1 °C", trayBackend.cpuTemp))
        if (trayBackend.gpuTemp > 0)
            parts.push(i18n("GPU: %1 °C", trayBackend.gpuTemp))
        if (trayBackend.activeProfileName)
            parts.push(i18n("Profile: %1", trayBackend.activeProfileName))
        return parts.join("  ·  ")
    }

    compactRepresentation: MouseArea {
        id: compactMouse

        property bool wasExpanded: false

        Layout.minimumWidth: height
        Layout.minimumHeight: width
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton

        onPressed: mouse => {
            if (mouse.button === Qt.LeftButton) {
                wasExpanded = main.expanded
            }
        }
        onClicked: mouse => {
            if (mouse.button === Qt.LeftButton) {
                main.expanded = !wasExpanded
            }
        }

        Kirigami.Icon {
            anchors.fill: parent
            source: Plasmoid.icon
            active: compactMouse.containsMouse
        }
    }

    fullRepresentation: PlasmaExtras.Representation {
        id: fullRep

        Layout.preferredWidth: Kirigami.Units.gridUnit * 28
        Layout.preferredHeight: Kirigami.Units.gridUnit * 24
        Layout.minimumWidth: Kirigami.Units.gridUnit * 28
        Layout.minimumHeight: Kirigami.Units.gridUnit * 24
        Layout.maximumWidth: Kirigami.Units.gridUnit * 28
        Layout.maximumHeight: Kirigami.Units.gridUnit * 24

        collapseMarginsHint: true

        header: PlasmaExtras.PlasmoidHeading {
            RowLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                PC3.TabBar {
                    id: tabBar
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    PC3.TabButton { text: i18n("Dashboard") }
                    PC3.TabButton { text: i18n("Profile") }
                    PC3.TabButton {
                        text: i18n("Water Cooler")
                        visible: trayBackend.waterCoolerSupported
                    }
                    PC3.TabButton { text: i18n("Hardware") }
                }

                PC3.ToolButton {
                    icon.name: "window-new"
                    onClicked: trayBackend.openControlCenter()
                    PC3.ToolTip { text: i18n("Open full Control Center") }
                }

                PC3.ToolButton {
                    icon.name: "view-refresh"
                    onClicked: trayBackend.refreshAll()
                    PC3.ToolTip { text: i18n("Refresh") }
                }
            }
        }

        contentItem: StackLayout {
            currentIndex: tabBar.currentIndex

            DashboardTab    { backend: trayBackend }
            PowerProfileTab { backend: trayBackend }
            WaterCoolerTab  { backend: trayBackend }
            HardwareTab     { backend: trayBackend }
        }

        footer: PlasmaExtras.PlasmoidHeading {
            RowLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                PC3.Label {
                    text: trayBackend.connected ? i18n("✓ Connected") : i18n("✗ Disconnected")
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: trayBackend.connected
                           ? Kirigami.Theme.positiveTextColor
                           : Kirigami.Theme.negativeTextColor
                }

                Item { Layout.fillWidth: true }

                PC3.Label {
                    text: trayBackend.powerState ? i18n("Power: %1", trayBackend.powerState) : ""
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    opacity: 0.7
                }
            }
        }
    }
}
