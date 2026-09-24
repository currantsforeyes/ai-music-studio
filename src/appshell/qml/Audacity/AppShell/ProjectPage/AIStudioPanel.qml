/*
 * Audacity: A Digital Audio Editor
 */
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
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
    property string scoreMode: "full"
    property string assistantMode: "cloud"
    property int panelTab: 0

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

    function localFile(url) {
        return decodeURIComponent(url.toString().replace(/^file:\/\/\//, ""))
    }

    function samplingMap() {
        const values = {
            "abc_temperature": abcTempField.text,
            "abc_top_p": abcTopPField.text,
            "abc_top_k": abcTopKField.text,
            "abc_repetition_penalty": abcRepField.text,
            "semantic_temperature": semTempField.text,
            "semantic_top_p": semTopPField.text,
            "semantic_top_k": semTopKField.text,
            "semantic_repetition_penalty": semRepField.text
        }
        const sparse = {}
        for (const key in values) {
            if (values[key].trim().length > 0) {
                sparse[key] = values[key]
            }
        }
        return sparse
    }

    // Enabling the per-project AI workspace is automatic once the project is saved.
    Component.onCompleted: {
        AIStudioStatus.enableProjectWorkspace()
        if (AIStudioStatus.assistantMode.length > 0) {
            root.assistantMode = AIStudioStatus.assistantMode
        }
    }

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

    FileDialog {
        id: importPromptDialog
        title: qsTrc("aistudio", "Import prompt")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTrc("aistudio", "Prompt files (*.json *.yaml *.yml)")]
        onAccepted: AIStudioStatus.importPromptFile(root.localFile(selectedFile))
    }

    FileDialog {
        id: assistantRunnerDialog
        title: qsTrc("aistudio", "Select the local runner executable")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTrc("aistudio", "Executable (*.exe)"), qsTrc("aistudio", "All files (*)")]
        onAccepted: assistantRunnerField.text = root.localFile(selectedFile)
    }

    FileDialog {
        id: assistantModelDialog
        title: qsTrc("aistudio", "Select the local model file")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTrc("aistudio", "Model files (*.gguf)"), qsTrc("aistudio", "All files (*)")]
        onAccepted: assistantModelFileField.text = root.localFile(selectedFile)
    }

    FileDialog {
        id: exportPromptDialog
        title: qsTrc("aistudio", "Export prompt")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTrc("aistudio", "Prompt JSON (*.json)"), qsTrc("aistudio", "Prompt YAML (*.yaml *.yml)")]
        onAccepted: AIStudioStatus.exportPromptFile(root.localFile(selectedFile), styleField.text, lyricsField.text,
                                                    titleField.text, seedField.text, root.scoreMode, stepsField.text,
                                                    guidanceField.text, root.samplingMap())
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                FlatButton {
                    text: qsTrc("aistudio", "Studio")
                    enabled: root.panelTab !== 0
                    onClicked: root.panelTab = 0
                }
                FlatButton {
                    text: qsTrc("aistudio", "Settings")
                    enabled: root.panelTab !== 1
                    onClicked: root.panelTab = 1
                }
                Item { Layout.fillWidth: true }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                visible: root.panelTab === 0

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

            // ---------- Song Plan (opens in a window; too large for the panel) ----------
            FlatButton {
                Layout.fillWidth: true
                text: AIStudioStatus.planDetail.loaded === true
                      ? qsTrc("aistudio", "Song Plan — %1 sections").arg(AIStudioStatus.planDetail.sections.length)
                      : qsTrc("aistudio", "Song Plan…")
                onClicked: songPlanPopup.open()
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

            // ---------- Create ----------
            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Create")
                font: ui.theme.bodyBoldFont
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                FlatButton {
                    text: qsTrc("aistudio", "Import prompt…")
                    onClicked: importPromptDialog.open()
                }
                FlatButton {
                    text: qsTrc("aistudio", "Export prompt…")
                    onClicked: exportPromptDialog.open()
                }
                Item { Layout.fillWidth: true }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Examples")
                font: ui.theme.bodyBoldFont
            }

            GridView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(contentHeight, 130)
                clip: true
                cellWidth: width / 2
                cellHeight: 34
                model: AIStudioStatus.examples

                delegate: FlatButton {
                    width: GridView.view.cellWidth - 6
                    text: modelData.name
                    onClicked: AIStudioStatus.loadExample(index)
                }
            }

            // Reload a saved generation's inputs (from "Reuse Prompt").
            Connections {
                target: AIStudioStatus
                function onPromptReuseChanged() {
                    titleField.text = AIStudioStatus.reuseTitle
                    styleField.text = AIStudioStatus.reuseStyle
                    lyricsField.text = AIStudioStatus.reuseLyrics
                    seedField.text = AIStudioStatus.reuseSeed
                    if (AIStudioStatus.reuseCot.length > 0) root.scoreMode = AIStudioStatus.reuseCot
                    stepsField.text = AIStudioStatus.reuseSteps
                    guidanceField.text = AIStudioStatus.reuseGuidance
                    const sampling = AIStudioStatus.reuseSampling || {}
                    abcTempField.text = sampling.abc_temperature !== undefined ? sampling.abc_temperature : ""
                    abcTopPField.text = sampling.abc_top_p !== undefined ? sampling.abc_top_p : ""
                    abcTopKField.text = sampling.abc_top_k !== undefined ? sampling.abc_top_k : ""
                    abcRepField.text = sampling.abc_repetition_penalty !== undefined ? sampling.abc_repetition_penalty : ""
                    semTempField.text = sampling.semantic_temperature !== undefined ? sampling.semantic_temperature : ""
                    semTopPField.text = sampling.semantic_top_p !== undefined ? sampling.semantic_top_p : ""
                    semTopKField.text = sampling.semantic_top_k !== undefined ? sampling.semantic_top_k : ""
                    semRepField.text = sampling.semantic_repetition_penalty !== undefined ? sampling.semantic_repetition_penalty : ""
                }
                function onCurrentSeedChanged() {
                    seedField.text = AIStudioStatus.currentSeed
                }
                function onAssistantResult(field, text) {
                    if (field === "style") {
                        styleField.text = text
                    } else if (field === "lyrics") {
                        lyricsField.text = text
                    }
                }
            }

            TextField {
                id: titleField
                Layout.fillWidth: true
                placeholderText: qsTrc("aistudio", "Song Title")
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
                    enabled: !AIStudioStatus.assistantBusy && lyricsField.text.trim().length > 0
                    onClicked: AIStudioStatus.createPrompt(lyricsField.text)
                }
                FlatButton {
                    text: qsTrc("aistudio", "Improve Prompt")
                    enabled: !AIStudioStatus.assistantBusy && styleField.text.trim().length > 0
                    onClicked: AIStudioStatus.improvePrompt(styleField.text)
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
                    enabled: !AIStudioStatus.assistantBusy && styleField.text.trim().length > 0
                    onClicked: AIStudioStatus.writeLyrics(styleField.text)
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StyledTextLabel { text: qsTrc("aistudio", "Score") }
                FlatButton {
                    text: qsTrc("aistudio", "Full")
                    enabled: root.scoreMode !== "full"
                    onClicked: root.scoreMode = "full"
                }
                FlatButton {
                    text: qsTrc("aistudio", "Melody")
                    enabled: root.scoreMode !== "melody"
                    onClicked: root.scoreMode = "melody"
                }
                FlatButton {
                    text: qsTrc("aistudio", "None")
                    enabled: root.scoreMode !== "off"
                    onClicked: root.scoreMode = "off"
                }
                Item { Layout.fillWidth: true }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 6

                StyledTextLabel { text: qsTrc("aistudio", "Seed") }
                TextField {
                    id: seedField
                    Layout.preferredWidth: 110
                    placeholderText: qsTrc("aistudio", "Random")
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                StyledTextLabel { text: qsTrc("aistudio", "Steps") }
                TextField {
                    id: stepsField
                    Layout.preferredWidth: 56
                    placeholderText: qsTrc("aistudio", "8")
                    inputMethodHints: Qt.ImhDigitsOnly
                }
                StyledTextLabel { text: qsTrc("aistudio", "Guidance") }
                TextField {
                    id: guidanceField
                    Layout.preferredWidth: 56
                    placeholderText: qsTrc("aistudio", "auto")
                }
                Item { Layout.fillWidth: true }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                text: qsTrc("aistudio", "Advanced sampling")
                font: ui.theme.bodyBoldFont
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                StyledTextLabel { Layout.preferredWidth: 58; text: qsTrc("aistudio", "ABC") }
                TextField { id: abcTempField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "temp") }
                TextField { id: abcTopPField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "top_p") }
                TextField { id: abcTopKField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "top_k") }
                TextField { id: abcRepField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "rep") }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 4

                StyledTextLabel { Layout.preferredWidth: 58; text: qsTrc("aistudio", "Semantic") }
                TextField { id: semTempField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "temp") }
                TextField { id: semTopPField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "top_p") }
                TextField { id: semTopKField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "top_k") }
                TextField { id: semRepField; Layout.fillWidth: true; placeholderText: qsTrc("aistudio", "rep") }
            }

            FlatButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTrc("aistudio", "Generate")
                enabled: lyricsField.text.trim().length > 0 && AIStudioStatus.modelConfigured
                onClicked: AIStudioStatus.runYue2Job(lyricsField.text, styleField.text, seedField.text, titleField.text,
                                                     root.scoreMode, stepsField.text, guidanceField.text, root.samplingMap())
            }

            
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                visible: root.panelTab === 1

                StyledTextLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    text: AIStudioStatus.workspaceStatus
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: ui.theme.strokeColor }

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "YuE2 runtime")
                    font: ui.theme.bodyBoldFont
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

                StyledTextLabel {
                    Layout.fillWidth: true
                    text: qsTrc("aistudio", "Writing assistant")
                    font: ui.theme.bodyBoldFont
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 6

                    StyledTextLabel { text: qsTrc("aistudio", "Engine") }
                    FlatButton {
                        text: qsTrc("aistudio", "Cloud")
                        enabled: root.assistantMode !== "cloud"
                        onClicked: root.assistantMode = "cloud"
                    }
                    FlatButton {
                        text: qsTrc("aistudio", "Local")
                        enabled: root.assistantMode !== "local"
                        onClicked: root.assistantMode = "local"
                    }
                    Item { Layout.fillWidth: true }
                }

                TextField {
                    id: assistantUrlField
                    Layout.fillWidth: true
                    visible: root.assistantMode === "cloud"
                    placeholderText: qsTrc("aistudio", "Base URL (OpenAI-compatible)")
                    Component.onCompleted: text = AIStudioStatus.assistantBaseUrl
                }

                TextField {
                    id: assistantModelField
                    Layout.fillWidth: true
                    visible: root.assistantMode === "cloud"
                    placeholderText: qsTrc("aistudio", "Model")
                    Component.onCompleted: text = AIStudioStatus.assistantModel
                }

                TextField {
                    id: assistantKeyField
                    Layout.fillWidth: true
                    visible: root.assistantMode === "cloud"
                    echoMode: TextInput.Password
                    placeholderText: AIStudioStatus.assistantHasKey
                                     ? qsTrc("aistudio", "API key (saved)")
                                     : qsTrc("aistudio", "API key")
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.assistantMode === "local"
                    spacing: 8

                    TextField {
                        id: assistantRunnerField
                        Layout.fillWidth: true
                        placeholderText: qsTrc("aistudio", "Runner executable (e.g. llama-server)")
                        Component.onCompleted: text = AIStudioStatus.assistantRunnerPath
                    }
                    FlatButton {
                        text: qsTrc("aistudio", "Browse…")
                        onClicked: assistantRunnerDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.assistantMode === "local"
                    spacing: 8

                    TextField {
                        id: assistantModelFileField
                        Layout.fillWidth: true
                        placeholderText: qsTrc("aistudio", "Model file (.gguf)")
                        Component.onCompleted: text = AIStudioStatus.assistantModelPath
                    }
                    FlatButton {
                        text: qsTrc("aistudio", "Browse…")
                        onClicked: assistantModelDialog.open()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: root.assistantMode === "local"
                    spacing: 8

                    StyledTextLabel { text: qsTrc("aistudio", "Port") }
                    TextField {
                        id: assistantPortField
                        Layout.preferredWidth: 90
                        placeholderText: qsTrc("aistudio", "8080")
                        inputMethodHints: Qt.ImhDigitsOnly
                        Component.onCompleted: text = String(AIStudioStatus.assistantPort)
                    }
                    Item { Layout.fillWidth: true }
                }

                FlatButton {
                    Layout.alignment: Qt.AlignLeft
                    text: qsTrc("aistudio", "Save")
                    onClicked: AIStudioStatus.setAssistantConfig(root.assistantMode, assistantUrlField.text, assistantModelField.text,
                                                                  assistantKeyField.text, assistantRunnerField.text,
                                                                  assistantModelFileField.text, Number(assistantPortField.text))
                }

                StyledTextLabel {
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                    opacity: 0.7
                    text: AIStudioStatus.assistantStatus.length > 0
                          ? AIStudioStatus.assistantStatus
                          : qsTrc("aistudio", "Cloud posts to any OpenAI-compatible endpoint; Local launches your runner with the chosen model file.")
                }
            }
        }
    }

    // The Song Plan is too large for the side panel, so it opens in a window.
    Popup {
        id: songPlanPopup

        property int planTab: 0
        readonly property real planTotal: {
            const sections = AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.sections : []
            let total = 0
            for (let index = 0; index < sections.length; ++index) {
                total = Math.max(total, sections[index].endSeconds)
            }
            return total > 0 ? total : 1
        }
        readonly property int melodyLow: {
            const melody = AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.melody : []
            let low = 127
            for (let index = 0; index < melody.length; ++index) {
                low = Math.min(low, melody[index].midiPitch)
            }
            return low === 127 ? 48 : low
        }
        readonly property int melodyHigh: {
            const melody = AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.melody : []
            let high = 0
            for (let index = 0; index < melody.length; ++index) {
                high = Math.max(high, melody[index].midiPitch)
            }
            return high === 0 ? 72 : high
        }

        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(1240, parent ? parent.width - 60 : 1240)
        height: Math.min(860, parent ? parent.height - 60 : 860)
        modal: true
        focus: true
        padding: 0
        closePolicy: Popup.CloseOnEscape

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
                        Layout.fillWidth: true
                        elide: Text.ElideRight
                        text: AIStudioStatus.planDetail.loaded === true
                              ? (String(AIStudioStatus.planDetail.seed).length > 0
                                 ? qsTrc("aistudio", "Song Plan — %1 (rev %2) · seed %3")
                                       .arg(AIStudioStatus.planDetail.id)
                                       .arg(AIStudioStatus.planDetail.revision)
                                       .arg(AIStudioStatus.planDetail.seed)
                                 : qsTrc("aistudio", "Song Plan — %1 (rev %2)")
                                       .arg(AIStudioStatus.planDetail.id)
                                       .arg(AIStudioStatus.planDetail.revision))
                              : qsTrc("aistudio", "Song Plan")
                        font: ui.theme.headerBoldFont
                    }

                    FlatButton {
                        text: qsTrc("aistudio", "Regenerate")
                        enabled: AIStudioStatus.planDetail.loaded === true
                        onClicked: AIStudioStatus.regenerateFromPlan()
                    }

                    FlatButton {
                        text: qsTrc("aistudio", "Save revision")
                        enabled: AIStudioStatus.planDetail.loaded === true
                        onClicked: AIStudioStatus.savePlanRevision()
                    }

                    FlatButton {
                        text: qsTrc("global", "Close")
                        onClicked: songPlanPopup.close()
                    }
                }
            }

            StyledTextLabel {
                Layout.fillWidth: true
                Layout.margins: 18
                visible: AIStudioStatus.planDetail.loaded !== true
                wrapMode: Text.Wrap
                opacity: 0.7
                text: qsTrc("aistudio", "Select an AI-generated clip to see its song plan.")
            }

            ScrollView {
                id: songPlanScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: AIStudioStatus.planDetail.loaded === true
                clip: true
                contentWidth: availableWidth

                ColumnLayout {
                    width: songPlanScroll.availableWidth
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        TextField { id: planTempoField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "BPM") }
                        TextField { id: planKeyField; Layout.preferredWidth: 70; placeholderText: qsTrc("aistudio", "Key") }
                        TextField { id: planMeterField; Layout.preferredWidth: 60; placeholderText: qsTrc("aistudio", "4/4") }
                        FlatButton {
                            text: qsTrc("aistudio", "Set")
                            onClicked: AIStudioStatus.setPlanMetadata(Number(planTempoField.text), planKeyField.text, planMeterField.text)
                        }
                    }

                    StyledTextLabel { Layout.fillWidth: true; text: qsTrc("aistudio", "Timeline"); font: ui.theme.bodyBoldFont }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        clip: true

                        Repeater {
                            model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.sections : []
                            delegate: Rectangle {
                                x: (modelData.startSeconds / songPlanPopup.planTotal) * parent.width
                                width: Math.max(3, ((modelData.endSeconds - modelData.startSeconds) / songPlanPopup.planTotal) * parent.width - 2)
                                y: 2
                                height: parent.height - 4
                                radius: 3
                                color: ui.theme.accentColor
                                opacity: index % 2 === 0 ? 0.85 : 0.55

                                StyledTextLabel {
                                    anchors.fill: parent
                                    anchors.margins: 5
                                    elide: Text.ElideRight
                                    text: modelData.name
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 24
                        clip: true

                        Repeater {
                            model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.chords : []
                            delegate: StyledTextLabel {
                                x: Math.max(0, Math.min(parent.width - width, (modelData.startSeconds / songPlanPopup.planTotal) * parent.width))
                                text: modelData.symbol
                            }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 90
                        clip: true

                        Repeater {
                            model: AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.melody : []
                            delegate: Rectangle {
                                readonly property int span: Math.max(1, songPlanPopup.melodyHigh - songPlanPopup.melodyLow)
                                x: (modelData.startSeconds / songPlanPopup.planTotal) * parent.width
                                width: Math.max(2, (modelData.durationSeconds / songPlanPopup.planTotal) * parent.width)
                                height: 3
                                y: (1 - (modelData.midiPitch - songPlanPopup.melodyLow) / span) * (parent.height - height)
                                color: ui.theme.accentColor
                                opacity: 0.85
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6

                        FlatButton {
                            text: qsTrc("aistudio", "Sections (%1)").arg(AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.sections.length : 0)
                            enabled: songPlanPopup.planTab !== 0
                            onClicked: songPlanPopup.planTab = 0
                        }
                        FlatButton {
                            text: qsTrc("aistudio", "Chords (%1)").arg(AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.chords.length : 0)
                            enabled: songPlanPopup.planTab !== 1
                            onClicked: songPlanPopup.planTab = 1
                        }
                        FlatButton {
                            text: qsTrc("aistudio", "Melody (%1)").arg(AIStudioStatus.planDetail.loaded === true ? AIStudioStatus.planDetail.melody.length : 0)
                            enabled: songPlanPopup.planTab !== 2
                            onClicked: songPlanPopup.planTab = 2
                        }
                        Item { Layout.fillWidth: true }
                    }

                    StyledTextLabel {
                        Layout.fillWidth: true
                        visible: songPlanPopup.planTab === 0
                        text: qsTrc("aistudio", "Sections")
                        font: ui.theme.bodyBoldFont
                    }

                    Column {
                        Layout.fillWidth: true
                        visible: songPlanPopup.planTab === 0
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
                        visible: songPlanPopup.planTab === 0
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
                        visible: songPlanPopup.planTab === 1
                        text: qsTrc("aistudio", "Chords")
                        font: ui.theme.bodyBoldFont
                    }

                    Column {
                        Layout.fillWidth: true
                        visible: songPlanPopup.planTab === 1
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
                        visible: songPlanPopup.planTab === 1
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
                        visible: songPlanPopup.planTab === 2
                        text: qsTrc("aistudio", "Melody")
                        font: ui.theme.bodyBoldFont
                    }

                    Column {
                        Layout.fillWidth: true
                        visible: songPlanPopup.planTab === 2
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
                        visible: songPlanPopup.planTab === 2
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
            }
        }
    }
}