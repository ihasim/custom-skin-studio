import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Item {
    id: vpanel
    property int fileLine: -1
    property int expanded: Window.window ? Window.window.expandVoice : -1
    readonly property var lines: AppController.voice

    FileDialog {
        id: fileDlg
        title: "Ses dosyası seç"
        nameFilters: ["Ses dosyaları (*.wav *.mp3 *.ogg *.opus *.m4a *.flac *.aac *.wem)", "Tüm dosyalar (*)"]
        onAccepted: { AppController.voiceFromFile(vpanel.fileLine, selectedFile); vpanel.expanded = vpanel.fileLine }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18
        spacing: 12
        Column {
            spacing: 4
            Text { text: "Sesler"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
            Text { width: vpanel.width - 36; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 12
                   text: "Her satıra bir ses dosyası sürükle, mikrofonla kaydet ya da sessize al. Dinle'ye basınca ses, bağlı olduğu animasyonla birlikte çalar." }
        }
        Text {
            visible: AppController.voiceNote !== ""
            Layout.fillWidth: true; wrapMode: Text.Wrap
            text: AppController.voiceNote; color: Theme.warn; font.pixelSize: 12
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; contentWidth: availableWidth
            Column {
                width: parent.width
                spacing: 8
                Repeater {
                    model: vpanel.lines
                    delegate: Column {
                        id: item
                        required property var modelData
                        required property int index
                        readonly property bool first: index === 0 || vpanel.lines[index - 1].group !== modelData.group
                        readonly property bool open: vpanel.expanded === modelData.index && modelData.mode === "replace"
                        readonly property bool rec: AppController.recordingLine === modelData.index
                        readonly property bool playing: AppController.playingLine === modelData.index
                        width: parent.width
                        spacing: 8
                        Row {
                            visible: item.first
                            spacing: 8; topPadding: item.index === 0 ? 0 : 10
                            Rectangle { width: 30; height: 30; radius: 8; color: Theme.panel2; border.color: Theme.lineStrong
                                Text { anchors.centerIn: parent; text: "PQWER".indexOf(item.modelData.group) >= 0 ? item.modelData.group : "•"; color: Theme.accent; font.pixelSize: 14; font.weight: Font.Bold } }
                            Text { anchors.verticalCenter: parent.verticalCenter; text: Theme.groupTitle(item.modelData.group); color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold }
                        }
                        Rectangle {
                            id: card
                            width: parent.width
                            height: cardCol.implicitHeight + 24
                            radius: 12
                            color: drop.containsDrag ? Qt.rgba(0.18, 0.83, 0.81, 0.10) : item.modelData.mode === "keep" ? Theme.bg2 : Theme.panel2
                            border.color: drop.containsDrag || item.rec ? Theme.accent : item.modelData.mode === "replace" ? Theme.accentDim : item.modelData.mode === "mute" ? "#5a2b33" : Theme.line
                            border.width: drop.containsDrag || item.rec ? 2 : 1
                            DropArea {
                                id: drop
                                anchors.fill: parent
                                onDropped: (d) => { if (d.hasUrls) { AppController.voiceFromFile(item.modelData.index, d.urls[0]); vpanel.expanded = item.modelData.index } }
                            }
                            Column {
                                id: cardCol
                                x: 12; y: 12; width: parent.width - 24
                                spacing: 10
                                RowLayout {
                                    width: parent.width; spacing: 8
                                    Column {
                                        Layout.fillWidth: true
                                        Text { text: item.modelData.label; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                                        Text { width: parent.width; elide: Text.ElideRight; color: Theme.faint; font.pixelSize: 11
                                               text: item.modelData.event + (item.modelData.variants > 1 ? " · " + item.modelData.variants + " varyasyon" : "") }
                                    }
                                    Rectangle {
                                        height: 22; radius: 11; width: chip.implicitWidth + 16
                                        color: item.modelData.mode === "replace" ? Qt.rgba(0.18, 0.83, 0.81, 0.15) : item.modelData.mode === "mute" ? Qt.rgba(0.94, 0.39, 0.39, 0.15) : Theme.panel
                                        Text { id: chip; anchors.centerIn: parent; font.pixelSize: 11
                                               color: item.modelData.mode === "replace" ? Theme.accent : item.modelData.mode === "mute" ? Theme.danger : Theme.muted
                                               text: item.modelData.mode === "replace" ? "Yeni ses · " + item.modelData.outDur.toFixed(1) + " sn"
                                                   : item.modelData.mode === "mute" ? "Sessiz"
                                                   : item.modelData.originalSilent ? "Mevcut · sessiz" : item.modelData.hasOriginal ? "Mevcut · " + item.modelData.originalDur.toFixed(1) + " sn" : "Mevcut" }
                                    }
                                }
                                // waveform of what plays now
                                WaveformItem {
                                    width: parent.width; height: 30
                                    visible: item.modelData.mode !== "mute" && !(item.modelData.mode === "keep" && item.modelData.originalSilent) && peaks.length > 0
                                    peaks: AppController.voicePeaks(item.modelData.index, 90)
                                    color: Theme.lineStrong
                                    playedColor: Theme.accent
                                    progress: item.playing ? AppController.playProgress : 0
                                }
                                Row {
                                    spacing: 6
                                    SButton { small: true; glyph: item.playing ? "■" : "▶"; text: item.playing ? "Durdur" : "Dinle"
                                              enabled: item.modelData.mode === "replace" || (item.modelData.mode === "keep" && item.modelData.hasOriginal && !item.modelData.originalSilent)
                                              onClicked: item.playing ? AppController.stopVoice() : AppController.playVoice(item.modelData.index, true) }
                                    SButton { small: true; glyph: "⇪"; text: "Dosya"; onClicked: { vpanel.fileLine = item.modelData.index; fileDlg.open() } }
                                    SButton { small: true; kind: item.rec ? "danger" : "ghost"; glyph: item.rec ? "■" : "●"; text: item.rec ? "Bitir" : "Kaydet"
                                              onClicked: { if (item.rec) { AppController.stopRecording(); vpanel.expanded = item.modelData.index } else AppController.startRecording(item.modelData.index) } }
                                    SButton { small: true; checkable: true; checked: item.modelData.mode === "mute"; text: "Sessiz"
                                              onClicked: AppController.setVoiceMode(item.modelData.index, item.modelData.mode === "mute" ? (item.modelData.hasSource ? "replace" : "keep") : "mute") }
                                    SButton { small: true; kind: "subtle"; text: "↺"; visible: item.modelData.mode !== "keep"
                                              ToolTip.visible: hovered; ToolTip.text: "Mevcut sese dön"
                                              onClicked: AppController.setVoiceMode(item.modelData.index, "keep") }
                                }
                                // recording level
                                Rectangle {
                                    visible: item.rec
                                    width: parent.width; height: 8; radius: 4; color: Theme.line
                                    Rectangle { width: parent.width * Math.min(1, AppController.recordLevel * 1.4); height: 8; radius: 4; color: Theme.danger
                                                Behavior on width { NumberAnimation { duration: 60 } } }
                                }
                                Text { visible: item.rec; text: "Kaydediliyor… konuş, bitince Bitir'e bas."; color: Theme.danger; font.pixelSize: 12 }
                                Text {
                                    visible: item.modelData.mode === "replace" && !item.open
                                    text: "Kırpma ve ses ayarı ▾"; color: Theme.accent; font.pixelSize: 12
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: vpanel.expanded = item.modelData.index }
                                }
                                // editor
                                Column {
                                    visible: item.open
                                    width: parent.width; spacing: 10
                                    Text { text: "Kayıttan kullanılacak kısım"; color: Theme.muted; font.pixelSize: 12 }
                                    Item {
                                        id: trim
                                        width: parent.width; height: 54
                                        readonly property real dur: Math.max(0.01, item.modelData.sourceDur)
                                        function xOf(t) { return t / dur * width }
                                        function tOf(x) { return Math.max(0, Math.min(dur, x / width * dur)) }
                                        Rectangle { anchors.fill: parent; radius: 6; color: Theme.bg }
                                        WaveformItem { anchors.fill: parent; anchors.margins: 4; peaks: item.open ? AppController.sourcePeaks(item.modelData.index, 120) : []; color: Theme.lineStrong; playedColor: Theme.lineStrong }
                                        Rectangle { x: trim.xOf(item.modelData.trimStart); width: trim.xOf(item.modelData.trimEnd) - x; height: parent.height
                                                    color: Qt.rgba(0.18, 0.83, 0.81, 0.14); border.color: Theme.accent; radius: 4 }
                                        Repeater {
                                            model: ["trimStart", "trimEnd"]
                                            delegate: Rectangle {
                                                x: trim.xOf(item.modelData[modelData]) - 5; y: -3; width: 10; height: trim.height + 6; radius: 4; color: Theme.accent
                                                MouseArea {
                                                    anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.SizeHorCursor
                                                    onPositionChanged: (m) => { if (pressed) AppController.setVoiceEdit(item.modelData.index, modelData, trim.tOf(mapToItem(trim, m.x, 0).x)) }
                                                }
                                            }
                                        }
                                    }
                                    Row {
                                        spacing: 6
                                        Text { anchors.verticalCenter: parent.verticalCenter; color: Theme.faint; font.pixelSize: 11
                                               text: item.modelData.trimStart.toFixed(2) + " – " + item.modelData.trimEnd.toFixed(2) + " sn" }
                                        SButton { small: true; kind: "subtle"; glyph: "✂"; text: "Sessizliği otomatik kırp"; onClicked: AppController.autoTrim(item.modelData.index) }
                                    }
                                    SSlider { width: parent.width; label: "Ses seviyesi"; from: -12; to: 12; neutral: 0; bipolar: true; value: item.modelData.gainDb
                                              format: function(v) { return (v > 0 ? "+" : "") + v.toFixed(1) + " dB" }
                                              onMoved: (v) => AppController.setVoiceEdit(item.modelData.index, "gainDb", v) }
                                    SSlider { width: parent.width; label: "Yumuşak giriş"; from: 0; to: 300; neutral: 15; value: item.modelData.fadeIn
                                              format: function(v) { return Math.round(v) + " ms" }
                                              onMoved: (v) => AppController.setVoiceEdit(item.modelData.index, "fadeIn", v) }
                                    SSlider { width: parent.width; label: "Yumuşak çıkış"; from: 0; to: 600; neutral: 60; value: item.modelData.fadeOut
                                              format: function(v) { return Math.round(v) + " ms" }
                                              onMoved: (v) => AppController.setVoiceEdit(item.modelData.index, "fadeOut", v) }
                                    SToggle { text: "Ses yüksekliğini oyuna göre eşitle (−13 LUFS)"; checked: item.modelData.normalize
                                              onToggled: AppController.setVoiceEdit(item.modelData.index, "normalize", checked ? 1 : 0) }
                                    Text { text: "Kapat ▴"; color: Theme.muted; font.pixelSize: 12
                                           MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: vpanel.expanded = -1 } }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
