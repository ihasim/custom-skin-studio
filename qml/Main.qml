import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import SkinStudio

ApplicationWindow {
    id: win
    width: 1440; height: 900
    minimumWidth: 1100; minimumHeight: 700
    visible: true
    title: AppController.fileName ? AppController.fileName + " — Custom Skin Studio" : "Custom Skin Studio"
    color: Theme.bg
    font.family: Theme.font

    property string mode: "colors"
    property int expandVoice: -1
    function openCrop(url) { mode = "loadscreen"; crop.openWith(url) }

    StartScreen { anchors.fill: parent; visible: AppController.screen !== "editor" }
    Editor { id: editor; anchors.fill: parent; visible: AppController.screen === "editor"; mode: win.mode; onModeRequested: (m) => win.mode = m }

    CropDialog { id: crop; anchors.fill: parent }

    // a .fantome dropped anywhere opens it
    DropArea {
        anchors.fill: parent
        enabled: AppController.screen === "editor"
        keys: ["text/uri-list"]
        onDropped: (drop) => {
            if (drop.hasUrls && drop.urls[0].toString().toLowerCase().endsWith(".fantome")) {
                if (AppController.dirty) { pendingOpen.url = drop.urls[0]; pendingOpen.open() }
                else AppController.openFantome(drop.urls[0])
            }
        }
    }

    Dialog {
        id: pendingOpen
        property url url
        anchors.centerIn: parent
        modal: true
        title: "Kaydedilmemiş değişiklikler var"
        standardButtons: Dialog.Discard | Dialog.Cancel
        Text { text: "Yeni paketi açarsan şimdiki değişiklikler kaybolur."; color: Theme.text }
        onDiscarded: { close(); AppController.openFantome(url) }
    }

    // error toast
    Rectangle {
        visible: AppController.error !== ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        width: Math.min(parent.width - 48, errText.implicitWidth + 80); height: Math.max(44, errText.implicitHeight + 20)
        radius: 10; color: "#3a1c22"; border.color: Theme.danger
        Text {
            id: errText
            anchors.left: parent.left; anchors.leftMargin: 16; anchors.right: closeErr.left; anchors.rightMargin: 8
            anchors.verticalCenter: parent.verticalCenter
            text: AppController.error; color: Theme.text; wrapMode: Text.Wrap; font.pixelSize: 13
        }
        SIcon { id: closeErr; name: "close"; color: Theme.muted; width: 16; height: 16; anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter
            MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: AppController.clearError() } }
        Timer { running: parent.visible; interval: 9000; onTriggered: AppController.clearError() }
    }
}
