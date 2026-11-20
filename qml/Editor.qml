import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Rectangle {
    id: ed
    color: Theme.bg
    property string mode: "colors"
    property bool overlayOpen: false
    signal modeRequested(string m)

    FileDialog {
        id: saveDlg
        title: "Yeni .fantome olarak kaydet"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Skin paketi (*.fantome)"]
        currentFolder: AppController.saveFolder()
        selectedFile: AppController.saveFolder() + "/" + AppController.suggestedSaveName
        onAccepted: AppController.save(selectedFile)
    }
    FileDialog { id: openDlg; nameFilters: ["Skin paketi (*.fantome)"]; onAccepted: AppController.openFantome(selectedFile) }
    FolderDialog { id: gameDlg; title: "League of Legends klasörünü seç"; onAccepted: AppController.setGameDir(selectedFolder) }

    Connections {
        target: AppController
        function onSaved(path, report) { savedPopup.path = path; savedPopup.report = report; savedPopup.open() }
    }

    // ---------------- top bar
    Rectangle {
        id: top
        anchors.left: parent.left; anchors.right: parent.right
        height: 56
        color: Theme.bg2
        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
        RowLayout {
            anchors.fill: parent; anchors.leftMargin: 18; anchors.rightMargin: 14
            spacing: 14
            Text { text: "Custom Skin Studio"; color: Theme.accent; font.pixelSize: 15; font.weight: Font.Bold }
            Rectangle { width: 1; height: 22; color: Theme.line }
            Column {
                Layout.fillWidth: true
                Text { text: AppController.pkgName || AppController.fileName; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold; elide: Text.ElideRight; width: parent.width }
                Text { text: AppController.champion + " · " + AppController.fileName + (AppController.pkgVersion ? " · v" + AppController.pkgVersion : ""); color: Theme.faint; font.pixelSize: 11; elide: Text.ElideRight; width: parent.width }
            }
            Text { text: AppController.status; color: Theme.muted; font.pixelSize: 12; elide: Text.ElideRight; Layout.maximumWidth: 360 }
            Rectangle {
                height: 30; radius: 15; color: Theme.panel; border.color: Theme.line
                implicitWidth: gameRow.implicitWidth + 22
                Row { id: gameRow; anchors.centerIn: parent; spacing: 7
                    Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: AppController.gameFound ? Theme.ok : Theme.warn }
                    Text { text: AppController.gameFound ? "Oyun dosyaları bağlı" : "Oyun klasörü seç"; color: Theme.muted; font.pixelSize: 12 }
                }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: gameDlg.open(); ToolTip.visible: containsMouse; hoverEnabled: true; ToolTip.text: AppController.gameDir }
            }
            SButton { text: "Aç"; glyph: ""; small: false; onClicked: openDlg.open() }
            SButton { kind: "primary"; text: AppController.dirty ? "Yeni sürüm olarak kaydet" : "Kaydet"; enabled: AppController.dirty; onClicked: saveDlg.open() }
        }
    }

    // ---------------- mode rail
    Rectangle {
        id: rail
        anchors.top: top.bottom; anchors.bottom: parent.bottom; anchors.left: parent.left
        width: 84
        color: Theme.bg2
        Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.line }
        Column {
            anchors.top: parent.top; anchors.topMargin: 14
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 6
            Repeater {
                model: [{k: "colors", t: "Renkler"}, {k: "effects", t: "Efektler"}, {k: "voice", t: "Sesler"},
                        {k: "loadscreen", t: "Yükleme\nekranı"}, {k: "package", t: "Paket"}]
                delegate: Rectangle {
                    width: 70; height: 64; radius: 12
                    readonly property bool on: ed.mode === modelData.k
                    color: on ? Theme.panel2 : ma.containsMouse ? Theme.panel : "transparent"
                    border.color: on ? Theme.accentDim : "transparent"
                    Column {
                        anchors.centerIn: parent; spacing: 5
                        SIcon { name: modelData.k; width: 22; height: 22; color: on ? Theme.accent : Theme.muted; anchors.horizontalCenter: parent.horizontalCenter }
                        Text { text: modelData.t; color: on ? Theme.text : Theme.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter; anchors.horizontalCenter: parent.horizontalCenter; lineHeight: 0.9 }
                    }
                    MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; cursorShape: Qt.PointingHandCursor; onClicked: ed.modeRequested(modelData.k) }
                }
            }
        }
    }

    // ---------------- right panel
    Rectangle {
        id: panel
        anchors.top: top.bottom; anchors.bottom: parent.bottom; anchors.right: parent.right
        width: Math.max(380, Math.min(460, ed.width * 0.3))
        color: Theme.panel
        Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Theme.line }
        Loader {
            anchors.fill: parent; anchors.leftMargin: 1
            sourceComponent: ed.mode === "effects" ? effectsC : ed.mode === "voice" ? voiceC : ed.mode === "loadscreen" ? lsC : ed.mode === "package" ? pkgC : colorsC
        }
        Component { id: colorsC; ColorsPanel {} }
        Component { id: effectsC; EffectsPanel {} }
        Component { id: voiceC; VoicePanel {} }
        Component { id: lsC; LoadscreenPanel { onSaveRequested: saveDlg.open() } }
        Component { id: pkgC; PackagePanel { onSaveRequested: saveDlg.open() } }
    }

    // ---------------- stage
    Item {
        anchors.top: top.bottom; anchors.bottom: parent.bottom; anchors.left: rail.right; anchors.right: panel.left
        Viewport { id: vp; anchors.fill: parent; anchors.bottomMargin: tl.height; pickEnabled: ed.mode === "colors"; inputEnabled: !ed.overlayOpen }
        Timeline { id: tl; anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom }
        Rectangle {
            visible: !AppController.gameFound
            anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 16
            width: warnRow.implicitWidth + 28; height: 40; radius: 10; color: "#2e2516"; border.color: Theme.warn
            Row { id: warnRow; anchors.centerIn: parent; spacing: 10
                Text { anchors.verticalCenter: parent.verticalCenter; text: "Oyun dosyaları bulunamadı: animasyon yok, model bekleme duruşunda."; color: Theme.text; font.pixelSize: 12 }
                SButton { small: true; text: "Klasör seç"; onClicked: gameDlg.open() } }
        }
    }

    Popup {
        id: savedPopup
        property string path
        property string report
        anchors.centerIn: Overlay.overlay
        modal: true
        padding: 24
        background: Rectangle { radius: 14; color: Theme.panel; border.color: Theme.line }
        contentItem: Column {
            spacing: 12; width: 440
            Row { spacing: 10; SIcon { name: "check"; color: Theme.ok; width: 22; height: 22 }
                  Text { text: "Paket kaydedildi"; color: Theme.text; font.pixelSize: 18; font.weight: Font.DemiBold } }
            Text { width: parent.width; text: savedPopup.path; color: Theme.muted; font.pixelSize: 12; wrapMode: Text.WrapAnywhere }
            Text { width: parent.width; text: "Değişenler: " + savedPopup.report; color: Theme.text; font.pixelSize: 13; wrapMode: Text.Wrap }
            Text { width: parent.width; text: "Windows'ta ROSE ile kur ve oyunda dene. Oyunda denenmeden hiçbir değişiklik doğrulanmış sayılmaz."; color: Theme.faint; font.pixelSize: 12; wrapMode: Text.Wrap }
            SButton { kind: "primary"; text: "Tamam"; onClicked: savedPopup.close() }
        }
    }
}
