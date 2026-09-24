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

    // Only YuE2 is implemented today; the other models render disabled.
    property string selectedModel: "yue2-native"
    property bool planExpanded: false

    readonly property string uploadedFileName: {
        const assets = AIStudioStatus.libraryAssets
        for (let index = assets.length - 1; index >= 0; --index) {
            if (assets[index].origin === "uploaded") {
                return assets[index].name
            }
        }
        return ""
    }

    function importWav() {
        const sourcePaths = wavPicker.selectFiles()
        for (let index = 0; index < sourcePaths.length; ++index) {
            AIStudioStatus.importLocalWav(sourcePaths[index])
        }
    }

    // Enabling the per-project AI workspace is automatic once the project is saved.
    Component.onCompleted: AIStudioStatus.enableProjectWorkspace()

    FilePickerModel {
        id: wavPicker
        title: qsTrc("aistudio", "Import audio into AI Library")
        filter: [qsTrc("aistudio", "Audio (*.wav)")]
    }

    FilePickerModel {
        id: cliPicker
        title: qsTrc("aistudio", "Select the YuE2 CLI executable")
        filter: [qsTrc("aistudio", "Executable (*.exe)"), qsTrc("aistudio", "All files (*)")]
    }

    FilePickerModel {
        id: modelPicker
        title: qsTrc("aistudio", "Select the YuE2 model folder")
    }

    ScrollView {
        id: panelScroll
        anchors.fill: parent
        anchors.margins: 16
        clip: true
        contentWidth: availableWidth

        ColumnLayout {
            width: panelScroll.availableWidth
            spacing: 12

            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "AI Studio")
                font: ui.theme.headerBoldFont
            }

            StyledTextLabel {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                visible: text.length > 0
                opacity: 0.8
                text: AIStudioStatus.workspaceStatus
            }

            // ---------- Select Model ----------
            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Select Model")
                font: ui.theme.bodyBoldFont
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                RoundedRadioButton {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "YuE2")
                    checked: root.selectedModel === "yue2-native"
                    onClicked: root.selectedModel = "yue2-native"
                }

                RoundedRadioButton {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "Stable Audio 3")
                    enabled: false
                }

                RoundedRadioButton {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "Stem Separation")
                    enabled: false
                }

                RoundedRadioButton {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "Voice Changer")
                    enabled: false
                }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.75
                text: AIStudioStatus.modelConfigured
                      ? qsTrc("aistudio", "YuE2 runtime ready")
                      : qsTrc("aistudio", "Choose the YuE2 CLI and model folder to enable generation")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StyledTextLabel { Layout.preferredWidth: 52; text: qsTrc("aistudio", "CLI") }
                StyledTextLabel {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    opacity: 0.8
                    text: AIStudioStatus.modelCliPath.length > 0
                          ? AIStudioStatus.modelCliPath
                          : qsTrc("aistudio", "Not set")
                }
                FlatButton {
                    text: qsTrc("aistudio", "Choose…")
                    onClicked: {
                        const path = cliPicker.selectFile()
                        if (path.length > 0) {
                            AIStudioStatus.setModelCliPath(path)
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StyledTextLabel { Layout.preferredWidth: 52; text: qsTrc("aistudio", "Model") }
                StyledTextLabel {
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                    opacity: 0.8
                    text: AIStudioStatus.modelModelPath.length > 0
                          ? AIStudioStatus.modelModelPath
                          : qsTrc("aistudio", "Not set")
                }
                FlatButton {
                    text: qsTrc("aistudio", "Choose…")
                    onClicked: {
                        const path = modelPicker.selectDirectory()
                        if (path.length > 0) {
                            AIStudioStatus.setModelModelPath(path)
                        }
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

            // ---------- Import ----------
            FlatButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTrc("aistudio", "Import Audio")
                onClicked: root.importWav()
            }

            StyledTextLabel {
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: root.uploadedFileName.length > 0
                      ? qsTrc("aistudio", "Filename of uploaded audio: %1").arg(root.uploadedFileName)
                      : qsTrc("aistudio", "Filename of uploaded audio")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                CheckBox {
                    text: qsTrc("aistudio", "Cover Version")
                    enabled: false
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "(coming soon)")
                    opacity: 0.6
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

            // ---------- Song Plan (collapsed behind a toggle, above Create) ----------
            FlatButton {
                Layout.fillWidth: true
                text: root.planExpanded
                      ? qsTrc("aistudio", "Song Plan  ▾")
                      : qsTrc("aistudio", "Song Plan  ▸")
                onClicked: root.planExpanded = !root.planExpanded
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 8
                visible: root.planExpanded

                ListView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(contentHeight, 120)
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
                    opacity: 0.7
                    text: qsTrc("aistudio", "Generated songs produce a plan with sections, chords and melody.")
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

                    TextField { id: planTempoField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "BPM") }
                    TextField { id: planKeyField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "Key") }
                    TextField { id: planMeterField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "4/4") }
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
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

            // ---------- Create ----------
            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Create")
                font: ui.theme.bodyBoldFont
            }

            ScrollView {
                id: styleScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 72
                clip: true

                TextArea {
                    id: styleField
                    width: styleScroll.availableWidth
                    wrapMode: TextArea.Wrap
                    placeholderText: qsTrc("aistudio", "Style Prompt")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FlatButton {
                    text: qsTrc("aistudio", "Create Prompt")
                    enabled: false
                }
                FlatButton {
                    text: qsTrc("aistudio", "Improve Prompt")
                    enabled: false
                }
                Item { Layout.fillWidth: true }
            }

            ScrollView {
                id: lyricsScroll
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                clip: true

                TextArea {
                    id: lyricsField
                    width: lyricsScroll.availableWidth
                    wrapMode: TextArea.Wrap
                    placeholderText: qsTrc("aistudio", "Lyrics\n[Verse]\n...\n[Chorus]\n...")
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FlatButton {
                    text: qsTrc("aistudio", "Write Lyrics")
                    enabled: false
                }
                Item { Layout.fillWidth: true }
            }

            FlatButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTrc("aistudio", "Generate")
                enabled: lyricsField.text.trim().length > 0 && AIStudioStatus.modelConfigured
                onClicked: AIStudioStatus.runYue2Job(lyricsField.text, styleField.text)
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

            // ---------- Assets ----------
            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Library — This Project")
                font: ui.theme.bodyBoldFont
            }

            ListView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 160)
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
                        visible: modelData.canAddToTimeline
                        text: qsTrc("aistudio", "Insert audio")
                        onClicked: AIStudioStatus.addLibraryAssetToTimeline(modelData.id)
                    }
                }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                visible: AIStudioStatus.libraryAssets.length === 0
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTrc("aistudio", "Imported and generated assets stay here even when they are not on the timeline.")
            }
        }
    }
}