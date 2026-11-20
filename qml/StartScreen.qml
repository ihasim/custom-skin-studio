import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SkinStudio

Rectangle {
    id: s
    color: Theme.bg

    FileDialog {
        id: openDlg
        title: ".fantome dosyası seç"
        nameFilters: ["Skin paketi (*.fantome)", "Tüm dosyalar (*)"]
        onAccepted: AppController.openFantome(selectedFile)
    }
    FolderDialog {
        id: gameDlg
        title: "League of Legends klasörünü seç"
        onAccepted: AppController.setGameDir(selectedFolder)
    }

    Column {
        anchors.centerIn: parent
        spacing: 22
        width: Math.min(640, s.width - 80)

        Column {
            spacing: 6
            anchors.horizontalCenter: parent.horizontalCenter
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Custom Skin Studio"; color: Theme.text; font.pixelSize: 30; font.weight: Font.Bold }
            Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Custom skin paketlerini aç, düzenle, yeni sürüm olarak kaydet"; color: Theme.muted; font.pixelSize: 14 }
        }

        Rectangle {
            id: drop
            width: parent.width; height: 300
            radius: 18
            color: dropArea.containsDrag ? Qt.rgba(0.18, 0.83, 0.81, 0.08) : Theme.bg2
            border.width: 2
            border.color: dropArea.containsDrag ? Theme.accent : Theme.lineStrong
            // dashed look
            Canvas {
                anchors.fill: parent; anchors.margins: 10
                onPaint: { var g = getContext("2d"); g.reset(); g.setLineDash([6, 6]); g.strokeStyle = "#2a3446"; g.lineWidth = 1.5;
                           g.beginPath(); g.roundedRect(1, 1, width - 2, height - 2, 12, 12); g.stroke() }
            }
            Column {
                anchors.centerIn: parent
                spacing: 16
                visible: AppController.screen !== "loading"
                SIcon { name: "upload"; width: 46; height: 46; color: dropArea.containsDrag ? Theme.accent : Theme.muted; anchors.horizontalCenter: parent.horizontalCenter }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Bir .fantome dosyası sürükle"; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "ya da"; color: Theme.faint; font.pixelSize: 13 }
                SButton { anchors.horizontalCenter: parent.horizontalCenter; kind: "primary"; text: "Dosya seç"; glyph: ""; onClicked: openDlg.open() }
            }
            Column {
                anchors.centerIn: parent
                spacing: 14
                visible: AppController.screen === "loading"
                BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: parent.visible }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: AppController.status; color: Theme.text; font.pixelSize: 15 }
                Text { anchors.horizontalCenter: parent.horizontalCenter; text: "Model, dokular, efektler ve sesler okunuyor"; color: Theme.faint; font.pixelSize: 12 }
            }
            DropArea {
                id: dropArea
                anchors.fill: parent
                onDropped: (d) => { if (d.hasUrls) AppController.openFantome(d.urls[0]) }
            }
            MouseArea { anchors.fill: parent; enabled: AppController.screen !== "loading"; cursorShape: Qt.PointingHandCursor; onClicked: openDlg.open(); z: -1 }
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            spacing: 8
            Rectangle { width: 8; height: 8; radius: 4; anchors.verticalCenter: parent.verticalCenter; color: AppController.gameDir ? Theme.ok : Theme.warn }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: AppController.gameDir ? "Oyun klasörü: " + AppController.gameDir : "Oyun klasörü seçilmedi (animasyonlar için gerekli)"
                color: Theme.muted; font.pixelSize: 12; elide: Text.ElideMiddle; width: Math.min(implicitWidth, 460)
            }
            SButton { small: true; kind: "subtle"; text: "Değiştir"; onClicked: gameDlg.open() }
        }
    }
}
