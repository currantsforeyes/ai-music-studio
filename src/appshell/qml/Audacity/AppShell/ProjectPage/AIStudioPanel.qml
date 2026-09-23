/*
 * Audacity: A Digital Audio Editor
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import Muse.Ui
import Muse.UiComponents
import Audacity.AIStudio

Item {
    id: root

    required property var navigationSection
    required property int navigationOrderStart

    function importWav() {
        const sourcePaths = wavPicker.selectFiles()
        for (let index = 0; index < sourcePaths.length; ++index) {
            AIStudioStatus.importLocalWav(sourcePaths[index])
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "AI Studio")
            font: ui.theme.headerBoldFont
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: AIStudioStatus.runtimeStatus
            font: ui.theme.bodyBoldFont
        }

        StyledTextLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: AIStudioStatus.workspaceStatus
        }

        StyledTextLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Run a deterministic local provider job to verify the authenticated job boundary. It writes a disposable WAV and manifest only; it does not alter this project.")
        }

        FlatButton {
            text: qsTrc("aistudio", "Enable project AI workspace")
            onClicked: AIStudioStatus.enableProjectWorkspace()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ui.theme.strokeColor
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "Library — This Project")
            font: ui.theme.bodyBoldFont
        }

        FilePickerModel {
            id: wavPicker
            title: qsTrc("aistudio", "Import WAV files into AI Library")
            filter: [qsTrc("aistudio", "WAV audio (*.wav)")]
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            FlatButton {
                text: qsTrc("aistudio", "Open Library")
                onClicked: libraryBrowser.open()
            }

            FlatButton {
                text: qsTrc("aistudio", "Import local WAV")
                onClicked: root.importWav()
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Library activity: %1").arg(AIStudioStatus.libraryStatus)
            font: ui.theme.bodyBoldFont
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 132)
            clip: true
            spacing: 6
            model: AIStudioStatus.libraryAssets
            delegate: Column {
                width: ListView.view.width
                spacing: 2

                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: modelData.name
                    font: ui.theme.bodyBoldFont
                }
                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: modelData.origin + " · " + modelData.status
                    font: ui.theme.bodyFont
                }
                FlatButton {
                    text: qsTrc("aistudio", "Add to Timeline")
                    enabled: modelData.canAddToTimeline
                    onClicked: AIStudioStatus.addLibraryAssetToTimeline(modelData.id)
                }
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.libraryAssets.length === 0
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Imported and generated assets stay here even when they are not on the timeline.")
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "Diagnostics")
            font: ui.theme.bodyBoldFont
        }

        FlatButton {
            text: qsTrc("aistudio", "Run test provider job")
            enabled: AIStudioStatus.runtimeStatus === qsTrc("aistudio", "Runtime host healthy")
            onClicked: AIStudioStatus.runTestJob()
        }

        FlatButton {
            text: qsTrc("aistudio", "Run worker-failure test")
            enabled: AIStudioStatus.runtimeStatus === qsTrc("aistudio", "Runtime host healthy")
            onClicked: AIStudioStatus.runWorkerFailureTest()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ui.theme.strokeColor
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "Jobs")
            font: ui.theme.bodyBoldFont
        }

        FlatButton {
            text: qsTrc("aistudio", "Refresh jobs")
            onClicked: AIStudioStatus.refreshJobs()
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 180)
            clip: true
            spacing: 6
            model: AIStudioStatus.jobs

            delegate: Column {
                width: ListView.view.width
                spacing: 2

                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: modelData.providerId + " · " + modelData.state
                    font: ui.theme.bodyBoldFont
                }
                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: modelData.message.length > 0 ? modelData.message : modelData.id
                    font: ui.theme.bodyFont
                }
                ProgressBar {
                    width: parent.width
                    from: 0
                    to: 1
                    value: modelData.progress
                }
                RowLayout {
                    spacing: 6

                    FlatButton {
                        text: qsTrc("aistudio", "Cancel")
                        enabled: modelData.state === "running" || modelData.state === "preparing"
                                 || modelData.state === "loading" || modelData.state === "queued"
                        onClicked: AIStudioStatus.cancelJob(modelData.id)
                    }
                    FlatButton {
                        text: qsTrc("aistudio", "Retry")
                        enabled: modelData.state === "failed" || modelData.state === "cancelled"
                                 || modelData.state === "interrupted"
                        onClicked: AIStudioStatus.retryJob(modelData.id)
                    }
                    FlatButton {
                        text: qsTrc("aistudio", "Insert")
                        enabled: modelData.state === "complete" && !modelData.inserted
                        onClicked: AIStudioStatus.insertJobOutput(modelData.id)
                    }
                }
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.jobs.length === 0
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "AI jobs appear here with live progress.")
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ui.theme.strokeColor
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "Song Plans")
            font: ui.theme.bodyBoldFont
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: planNameField
                Layout.fillWidth: true
                placeholderText: qsTrc("aistudio", "Plan name")
            }

            FlatButton {
                text: qsTrc("aistudio", "Create plan")
                onClicked: AIStudioStatus.createPlan(planNameField.text)
            }
        }

        FlatButton {
            text: qsTrc("aistudio", "Refresh plans")
            onClicked: AIStudioStatus.refreshPlans()
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 150)
            clip: true
            spacing: 6
            model: AIStudioStatus.plans

            delegate: Column {
                width: ListView.view.width
                spacing: 2

                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: modelData.id
                    font: ui.theme.bodyBoldFont
                }
                StyledTextLabel {
                    width: parent.width
                    elide: Text.ElideRight
                    text: qsTrc("aistudio", "rev %1 · %2 BPM · %3 · %4 sections")
                          .arg(modelData.revision).arg(modelData.tempo).arg(modelData.key).arg(modelData.sections)
                    font: ui.theme.bodyFont
                }
                FlatButton {
                    text: qsTrc("aistudio", "Edit")
                    onClicked: AIStudioStatus.loadPlan(modelData.id)
                }
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.plans.length === 0
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Song plans created here keep immutable revisions.")
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ui.theme.strokeColor
            visible: AIStudioStatus.planDetail.loaded === true
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            text: qsTrc("aistudio", "Edit %1 (rev %2)").arg(AIStudioStatus.planDetail.id).arg(AIStudioStatus.planDetail.revision)
            font: ui.theme.bodyBoldFont
        }

        RowLayout {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 6

            TextField { id: planTempoField; Layout.preferredWidth: 80; placeholderText: qsTrc("aistudio", "BPM") }
            TextField { id: planKeyField; Layout.preferredWidth: 80; placeholderText: qsTrc("aistudio", "Key") }
            TextField { id: planMeterField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "4/4") }
            FlatButton {
                text: qsTrc("aistudio", "Set")
                onClicked: AIStudioStatus.setPlanMetadata(Number(planTempoField.text), planKeyField.text, planMeterField.text)
            }
            FlatButton {
                text: qsTrc("aistudio", "Save revision")
                onClicked: AIStudioStatus.savePlanRevision()
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            text: qsTrc("aistudio", "Sections")
            font: ui.theme.bodyBoldFont
        }

        Column {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 2

            Repeater {
                model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.sections : []
                delegate: RowLayout {
                    width: parent.width
                    StyledTextLabel {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: modelData.name + "  " + modelData.startSeconds + "–" + modelData.endSeconds
                    }
                    FlatButton { text: qsTrc("aistudio", "Remove"); onClicked: AIStudioStatus.removePlanSection(index) }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 6

            TextField { id: sectionNameField; Layout.preferredWidth: 90; placeholderText: qsTrc("aistudio", "Name") }
            TextField { id: sectionStartField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "Start") }
            TextField { id: sectionEndField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "End") }
            FlatButton {
                text: qsTrc("aistudio", "Add section")
                onClicked: AIStudioStatus.addPlanSection(sectionNameField.text, Number(sectionStartField.text), Number(sectionEndField.text))
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            text: qsTrc("aistudio", "Chords")
            font: ui.theme.bodyBoldFont
        }

        Column {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 2

            Repeater {
                model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.chords : []
                delegate: RowLayout {
                    width: parent.width
                    StyledTextLabel {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: modelData.symbol + "  " + modelData.startSeconds + "–" + (modelData.startSeconds + modelData.durationSeconds)
                    }
                    FlatButton { text: qsTrc("aistudio", "Remove"); onClicked: AIStudioStatus.removePlanChord(index) }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 6

            TextField { id: chordSymbolField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "Chord") }
            TextField { id: chordStartField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "Start") }
            TextField { id: chordDurationField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "Dur") }
            FlatButton {
                text: qsTrc("aistudio", "Add chord")
                onClicked: AIStudioStatus.addPlanChord(chordSymbolField.text, Number(chordStartField.text), Number(chordDurationField.text))
            }
        }

        StyledTextLabel {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            text: qsTrc("aistudio", "Melody")
            font: ui.theme.bodyBoldFont
        }

        Column {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 2

            Repeater {
                model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.melody : []
                delegate: RowLayout {
                    width: parent.width
                    StyledTextLabel {
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: modelData.midiPitch + "  " + modelData.startSeconds + "s  " + modelData.lyric
                    }
                    FlatButton { text: qsTrc("aistudio", "Remove"); onClicked: AIStudioStatus.removePlanNote(index) }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: AIStudioStatus.planDetail.loaded === true
            spacing: 6

            TextField { id: notePitchField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "MIDI") }
            TextField { id: noteStartField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "Start") }
            TextField { id: noteDurationField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "Dur") }
            TextField { id: noteLyricField; Layout.preferredWidth: 80; placeholderText: qsTrc("aistudio", "Lyric") }
            FlatButton {
                text: qsTrc("aistudio", "Add note")
                onClicked: AIStudioStatus.addPlanNote(Number(notePitchField.text), Number(noteStartField.text), Number(noteDurationField.text), noteLyricField.text)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: ui.theme.strokeColor
        }

        StyledTextLabel {
            Layout.fillWidth: true
            text: qsTrc("aistudio", "Create")
            font: ui.theme.bodyBoldFont
        }

        StyledTextLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Set AI_YUE2_CLI and AI_YUE2_MODEL to enable the native YuE2 provider.")
        }

        TextArea {
            id: lyricsField
            Layout.fillWidth: true
            Layout.preferredHeight: 80
            wrapMode: TextArea.Wrap
            placeholderText: qsTrc("aistudio", "[Verse]\n...\n[Chorus]\n...")
        }

        TextField {
            id: styleField
            Layout.fillWidth: true
            placeholderText: qsTrc("aistudio", "Style")
        }

        FlatButton {
            text: qsTrc("aistudio", "Generate with YuE2")
            enabled: lyricsField.text.trim().length > 0
            onClicked: AIStudioStatus.runYue2Job(lyricsField.text, styleField.text)
        }

        StyledTextLabel {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: qsTrc("aistudio", "Planned: Create, Plan, Separate, Vocals, Instruments, and Jobs.")
        }

        Item { Layout.fillHeight: true }
    }

    Popup {
        id: libraryBrowser

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 800
        height: 500
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        property string filterKind: "all"
        property string selectedAssetId: ""
        property var displayedAssets: {
            const assets = AIStudioStatus.libraryAssets
            if (filterKind === "all") {
                return assets
            }
            return assets.filter(function(asset) {
                return filterKind === "imports" ? asset.origin === "uploaded" : asset.origin === "generated"
            })
        }
        property var selectedAsset: {
            for (let index = 0; index < displayedAssets.length; ++index) {
                if (displayedAssets[index].id === selectedAssetId) {
                    return displayedAssets[index]
                }
            }
            return null
        }

        background: Rectangle {
            color: ui.theme.backgroundPrimaryColor
            border.color: ui.theme.strokeColor
            border.width: 1
            radius: 4
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 54
                color: ui.theme.backgroundSecondaryColor

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 10
                    spacing: 10

                    StyledTextLabel {
                        text: qsTrc("aistudio", "Library — This Project")
                        font: ui.theme.headerBoldFont
                    }

                    Item { Layout.fillWidth: true }

                    FlatButton {
                        text: qsTrc("aistudio", "Import WAV")
                        onClicked: root.importWav()
                    }

                    FlatButton {
                        text: qsTrc("global", "Close")
                        onClicked: libraryBrowser.close()
                    }
                }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                Layout.leftMargin: 18
                Layout.rightMargin: 18
                Layout.topMargin: 10
                Layout.bottomMargin: 10
                text: qsTrc("aistudio", "This Project  /  AI Library")
                font: ui.theme.bodyBoldFont
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: ui.theme.backgroundPrimaryColor

                RowLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 150
                        color: ui.theme.backgroundSecondaryColor

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            StyledTextLabel {
                                Layout.fillWidth: true
                                text: qsTrc("aistudio", "Folders")
                                font: ui.theme.bodyBoldFont
                            }

                            Repeater {
                                model: [
                                    { key: "all", label: qsTrc("aistudio", "All assets") },
                                    { key: "imports", label: qsTrc("aistudio", "Imported WAV") },
                                    { key: "generated", label: qsTrc("aistudio", "Generated") }
                                ]

                                delegate: FlatButton {
                                    Layout.fillWidth: true
                                    text: modelData.label
                                    enabled: libraryBrowser.filterKind !== modelData.key
                                    onClicked: {
                                        libraryBrowser.filterKind = modelData.key
                                        libraryBrowser.selectedAssetId = ""
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }
                        }
                    }

                    Rectangle {
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1
                        color: ui.theme.strokeColor
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 12
                        spacing: 6

                        RowLayout {
                            Layout.fillWidth: true

                            StyledTextLabel { Layout.fillWidth: true; text: qsTrc("aistudio", "Name"); font: ui.theme.bodyBoldFont }
                            StyledTextLabel { Layout.preferredWidth: 82; text: qsTrc("aistudio", "Type"); font: ui.theme.bodyBoldFont }
                            StyledTextLabel { Layout.preferredWidth: 82; text: qsTrc("aistudio", "Status"); font: ui.theme.bodyBoldFont }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

                        ListView {
                            id: assetList
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 2
                            model: libraryBrowser.displayedAssets

                            delegate: Rectangle {
                                required property var modelData
                                width: ListView.view.width
                                height: 38
                                color: libraryBrowser.selectedAssetId === modelData.id ? ui.theme.accentColor : "transparent"
                                radius: 3

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 8
                                    anchors.rightMargin: 8
                                    spacing: 8

                                    StyledTextLabel { Layout.fillWidth: true; elide: Text.ElideRight; text: modelData.name }
                                    StyledTextLabel { Layout.preferredWidth: 82; elide: Text.ElideRight; text: modelData.kind }
                                    StyledTextLabel { Layout.preferredWidth: 82; elide: Text.ElideRight; text: modelData.status }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: libraryBrowser.selectedAssetId = modelData.id
                                }
                            }

                            StyledTextLabel {
                                anchors.centerIn: parent
                                visible: assetList.count === 0
                                text: qsTrc("aistudio", "No assets in this folder")
                            }
                        }

                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

                        RowLayout {
                            Layout.fillWidth: true

                            StyledTextLabel {
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                text: libraryBrowser.selectedAsset
                                      ? qsTrc("aistudio", "%1  ·  %2  ·  %3").arg(libraryBrowser.selectedAsset.name).arg(libraryBrowser.selectedAsset.origin).arg(libraryBrowser.selectedAsset.status)
                                      : qsTrc("aistudio", "Select an asset to view its details")
                            }

                            FlatButton {
                                text: qsTrc("aistudio", "Add to Timeline")
                                enabled: libraryBrowser.selectedAsset && libraryBrowser.selectedAsset.canAddToTimeline
                                onClicked: AIStudioStatus.addLibraryAssetToTimeline(libraryBrowser.selectedAsset.id)
                            }
                        }
                    }
                }
            }
        }
    }
}
