import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Item {
    id: ep
    property string pickKey: ""
    property color allColor: "transparent"

    ColorDialog {
        id: dlg
        onAccepted: { if (ep.pickKey === "*") { ep.allColor = selectedColor; AppController.setAllEffects(selectedColor) }
                      else AppController.setEffect(ep.pickKey, true, selectedColor) }
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18
        spacing: 14
        Column {
            spacing: 4
            Text { text: "Efektler"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
            Text { width: ep.width - 36; wrapMode: Text.Wrap; text: "Önce tek bir ana renk seç; istersen bir yeteneği (ör. R) ayrı renkte bırak."; color: Theme.muted; font.pixelSize: 12 }
        }
        Rectangle {
            Layout.fillWidth: true; implicitHeight: allCol.implicitHeight + 24
            radius: 12; color: Theme.bg2; border.color: Theme.line
            Column {
                id: allCol
                x: 12; y: 12; width: parent.width - 24; spacing: 10
                Text { text: "Bütün efektler"; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                Flow { width: parent.width; spacing: 8
                    Repeater { model: Theme.palette
                        delegate: SSwatch { swatch: modelData; selected: Qt.colorEqual(ep.allColor, modelData); onClicked: { ep.allColor = modelData; AppController.setAllEffects(modelData) } } }
                    Rectangle { width: 24; height: 24; radius: 12; color: "transparent"; border.color: Theme.lineStrong
                        Text { anchors.centerIn: parent; text: "+"; color: Theme.text; font.pixelSize: 15 }
                        MouseArea { anchors.fill: parent; onClicked: { ep.pickKey = "*"; dlg.selectedColor = ep.allColor; dlg.open() } } }
                }
                SButton { small: true; kind: "subtle"; glyph: "↺"; text: "Hepsini orijinaline döndür"; onClicked: AppController.resetEffects() }
            }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true; contentWidth: availableWidth
            Column {
                width: parent.width; spacing: 8
                Repeater {
                    model: AppController.effects
                    delegate: Rectangle {
                        id: card
                        required property var modelData
                        width: parent.width; height: 96 + (modelData.enabled ? 40 : 0)
                        radius: 12; color: modelData.enabled ? Theme.panel2 : Theme.bg2; border.color: modelData.enabled ? Theme.accentDim : Theme.line
                        opacity: modelData.hasColor ? 1 : 0.6
                        Rectangle {
                            id: badge
                            x: 12; y: 12; width: 44; height: 44; radius: 10
                            color: Qt.rgba(card.modelData.color.r, card.modelData.color.g, card.modelData.color.b, 0.18)
                            border.color: card.modelData.color
                            Text { anchors.centerIn: parent; text: card.modelData.key === "A" ? "AA" : card.modelData.key === "I" ? "✦" : card.modelData.key === "O" ? "…" : card.modelData.key
                                   color: card.modelData.color; font.pixelSize: 17; font.weight: Font.Bold }
                        }
                        // before -> after colour
                        Row {
                            x: card.width - 12 - width; y: 22; spacing: 4
                            visible: card.modelData.hasColor
                            Rectangle { width: 22; height: 22; radius: 11; color: card.modelData.original; border.color: Qt.rgba(1,1,1,0.15) }
                            Text { visible: card.modelData.enabled; text: "→"; color: Theme.faint; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
                            Rectangle { visible: card.modelData.enabled; width: 22; height: 22; radius: 11; color: card.modelData.color; border.color: Theme.text }
                        }
                        Column {
                            x: 68; y: 12; width: card.width - 68 - 80
                            spacing: 2
                            Text { text: card.modelData.label; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                            Text { width: parent.width; text: card.modelData.detail; color: Theme.faint; font.pixelSize: 11; elide: Text.ElideRight }
                            Text { text: card.modelData.systems + " efekt · " + card.modelData.textures + " doku"; color: Theme.faint; font.pixelSize: 11 }
                        }
                        Row {
                            x: 12; y: 64; spacing: 8
                            SToggle { text: card.modelData.enabled ? "Yeni renk" : "Orijinal"; checked: card.modelData.enabled; enabled: card.modelData.hasColor
                                      onToggled: AppController.setEffect(card.modelData.key, checked, card.modelData.color) }
                            SButton { small: true; kind: "subtle"; glyph: "▶"; text: "Önizle"; onClicked: AppController.previewEffect(card.modelData.key) }
                        }
                        Flow {
                            visible: card.modelData.enabled
                            x: 12; y: 100; width: card.width - 24; spacing: 7
                            Repeater { model: Theme.palette
                                delegate: SSwatch { width: 22; height: 22; swatch: modelData; selected: Qt.colorEqual(card.modelData.color, modelData)
                                                    onClicked: AppController.setEffect(card.modelData.key, true, modelData) } }
                            Rectangle { width: 22; height: 22; radius: 11; color: "transparent"; border.color: Theme.lineStrong
                                Text { anchors.centerIn: parent; text: "+"; color: Theme.text; font.pixelSize: 14 }
                                MouseArea { anchors.fill: parent; onClicked: { ep.pickKey = card.modelData.key; dlg.selectedColor = card.modelData.color; dlg.open() } } }
                        }
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true; wrapMode: Text.Wrap
            text: "Parçacıkların hareketi burada oynamaz; renkler efekt değerlerine ve dokularına uygulanır. Gerçek görüntüyü oyunda kontrol et."
            color: Theme.faint; font.pixelSize: 11
        }
    }
}
