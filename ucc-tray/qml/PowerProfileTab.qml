import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

/*
 * Profile tab — select profiles and adjust ODM performance mode.
 */
ScrollView {
    id: powerTab
    clip: true

    Flickable {
        contentHeight: col.implicitHeight + 24

        ColumnLayout {
            id: col
            width: powerTab.availableWidth
            spacing: 12

            anchors {
                left: parent.left
                right: parent.right
                top: parent.top
                margins: 16
            }

            // ── Profile Selection ──
            GroupBox {
                Layout.fillWidth: true

                ColumnLayout {
                    spacing: 8
                    width: parent.width

                    // ── System Profile ──
                    Label { text: "System Profile:"; font.pointSize: 9; opacity: 0.7 }
                    ComboBox {
                        id: profileCombo
                        Layout.fillWidth: true
                        // QStringList model: display texts are the names directly
                        model: backend.profileNames
                        currentIndex: backend.profileIds.indexOf(backend.activeProfileId)

                        Connections {
                            target: backend
                            function onActiveProfileChanged() {
                                var idx = backend.profileIds.indexOf(backend.activeProfileId)
                                if (idx >= 0 && profileCombo.currentIndex !== idx)
                                    profileCombo.currentIndex = idx
                            }
                            function onProfilesChanged() {
                                Qt.callLater(function() {
                                    profileCombo.currentIndex = backend.profileIds.indexOf(backend.activeProfileId)
                                })
                            }
                        }

                        onActivated: function(index) {
                            var id = backend.profileIds[index]
                            if (id && id !== backend.activeProfileId)
                                backend.setActiveProfile(id)
                        }
                    }

                    // ── Fan Profile ──
                    Label {
                        text: "Fan profile:"
                        font.pointSize: 9
                        opacity: 0.7
                        visible: backend.fanProfileIds.length > 0
                    }
                    ComboBox {
                        id: fanCombo
                        Layout.fillWidth: true
                        model: backend.fanProfileNames
                        visible: backend.fanProfileIds.length > 0
                        currentIndex: backend.fanProfileIds.indexOf(backend.activeProfileFanId)

                        Connections {
                            target: backend
                            function onActiveProfileChanged() {
                                var idx = backend.fanProfileIds.indexOf(backend.activeProfileFanId)
                                if (idx >= 0 && fanCombo.currentIndex !== idx)
                                    fanCombo.currentIndex = idx
                            }
                            function onFanProfilesChanged() {
                                Qt.callLater(function() {
                                    fanCombo.currentIndex = backend.fanProfileIds.indexOf(backend.activeProfileFanId)
                                })
                            }
                        }

                        onActivated: function(index) {
                            var id = backend.fanProfileIds[index]
                            if (id && id !== backend.activeProfileFanId)
                                backend.setActiveFanProfile(id)
                        }
                    }

                    // ── Keyboard Profile ──
                    Label {
                        text: "Keyboard profile:"
                        font.pointSize: 9
                        opacity: 0.7
                        visible: backend.keyboardProfileIds.length > 0
                    }
                    ComboBox {
                        id: kbCombo
                        Layout.fillWidth: true
                        model: backend.keyboardProfileNames
                        visible: backend.keyboardProfileIds.length > 0
                        currentIndex: {
                            var idx = backend.keyboardProfileIds.indexOf(backend.activeProfileKeyboardId)
                            return idx >= 0 ? idx : 0
                        }

                        Connections {
                            target: backend
                            function onActiveProfileChanged() {
                                var idx = backend.keyboardProfileIds.indexOf(backend.activeProfileKeyboardId)
                                kbCombo.currentIndex = idx >= 0 ? idx : 0
                            }
                            function onKeyboardProfilesChanged() {
                                Qt.callLater(function() {
                                    var idx = backend.keyboardProfileIds.indexOf(backend.activeProfileKeyboardId)
                                    kbCombo.currentIndex = idx >= 0 ? idx : 0
                                })
                            }
                        }

                        onActivated: function(index) {
                            var id = backend.keyboardProfileIds[index]
                            if (id) backend.setActiveKeyboardProfile(id)
                        }
                    }

                    Label {
                        text: "Switching profile applies it immediately."
                        font.pointSize: 9
                        opacity: 0.6
                    }
                }
            }

            // ── ODM Performance Profile ──
            GroupBox {
                title: "ODM Performance Mode"
                Layout.fillWidth: true
                visible: backend.availableODMProfiles.length > 0

                ColumnLayout {
                    spacing: 8
                    width: parent.width

                    ComboBox {
                        id: odmProfileCombo
                        Layout.fillWidth: true
                        model: backend.availableODMProfiles
                        currentIndex: Math.max(0, backend.availableODMProfiles.indexOf(backend.odmPerformanceProfile))

                        Connections {
                            target: backend
                            function onOdmPerformanceProfileChanged() {
                                odmProfileCombo.currentIndex = Math.max(0,
                                    backend.availableODMProfiles.indexOf(backend.odmPerformanceProfile));
                            }
                        }

                        onActivated: function(index) {
                            backend.setODMPerformanceProfile(model[index]);
                        }
                    }
                }
            }

        }
    }
}
