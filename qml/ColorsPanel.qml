import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Item {
    id: cp
    readonly property var part: AppController.parts[AppController.selectedPart]
    readonly property int sel: AppController.selectedRegion

    ColorDialog {
        id: colorDlg
        property int region: -1
        title: "Bölge rengi"
        onAccepted: AppController.setRegionColor(AppController.selectedPart, region, selectedColor)
    }

    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18
        spacing: 14

        Column {
            spacing: 4
            Text { text: "Renkler"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
            Text { text: "Modelde bir yere tıkla ya da aşağıdan bir bölge seç."; color: Theme.muted; font.pixelSize: 12 }
        }

        // part tabs
        Row {
            spacing: 6
            Repeater {
                model: AppController.parts
                delegate: SButton {
                    small: true
                    checkable: true
                    checked: AppController.selectedPart === index
                    text: modelData.label + (modelData.edited ? " •" : "")
                    onClicked: AppController.selectedPart = index
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true; Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            Column {
                width: parent.width
                spacing: 8
                Repeater {
                    model: cp.part ? cp.part.regions : []
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool open: cp.sel === index
                        width: parent.width
                        height: open ? body.implicitHeight + 72 : 58
                        radius: 12
                        color: open ? Theme.panel2 : hov.hovered ? Qt.lighter(Theme.panel, 1.15) : Theme.bg2
                        border.color: open ? Theme.accentDim : Theme.line
                        clip: true
                        Behavior on height { NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
                        HoverHandler { id: hov }

                        RowLayout {
                            id: head
                            x: 12; y: 0; width: parent.width - 24; height: 58
                            spacing: 10
                            Item { width: 40; height: 26
                                Rectangle { width: 26; height: 26; radius: 13; color: row.modelData.original; border.color: Qt.rgba(1,1,1,0.15) }
                                Rectangle { x: 14; width: 26; height: 26; radius: 13; color: row.modelData.color; border.color: Theme.bg; border.width: 2; visible: row.modelData.edited }
                            }
                            Column {
                                Layout.fillWidth: true
                                Text { text: row.modelData.name; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                                Text { text: "%" + (row.modelData.share * 100).toFixed(1) + " alan" + (row.modelData.edited ? " · değişti" : ""); color: row.modelData.edited ? Theme.accent : Theme.faint; font.pixelSize: 11 }
                            }
                            SIcon { name: "chevron"; width: 14; height: 14; color: Theme.muted; rotation: row.open ? 180 : 0 }
                        }
                        MouseArea { x: 0; y: 0; width: parent.width; height: 58; cursorShape: Qt.PointingHandCursor
                                    onClicked: AppController.selectedRegion = row.open ? -1 : row.index }

                        Column {
                            id: body
                            visible: row.open
                            x: 14; y: 62; width: parent.width - 28
                            spacing: 10
                            Text { text: "Hazır renkler"; color: Theme.muted; font.pixelSize: 12 }
                            Flow {
                                width: parent.width; spacing: 8
                                Repeater { model: Theme.palette
                                    delegate: SSwatch { swatch: modelData; onClicked: AppController.setRegionColor(AppController.selectedPart, row.index, modelData) } }
                                Rectangle { width: 24; height: 24; radius: 12; color: "transparent"; border.color: Theme.lineStrong
                                    Text { anchors.centerIn: parent; text: "+"; color: Theme.text; font.pixelSize: 15 }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: { colorDlg.region = row.index; colorDlg.selectedColor = row.modelData.color; colorDlg.open() } } }
                            }
                            SSlider { width: parent.width; label: "Ton"; from: -180; to: 180; neutral: 0; bipolar: true; value: row.modelData.hue
                                      format: function(v) { return (v > 0 ? "+" : "") + Math.round(v) + "°" }
                                      onMoved: (v) => AppController.setRegion(AppController.selectedPart, row.index, "hue", v) }
                            SSlider { width: parent.width; label: "Doygunluk"; from: 0; to: 2.5; neutral: 1; bipolar: true; value: row.modelData.sat
                                      format: function(v) { return "%" + Math.round(v * 100) }
                                      onMoved: (v) => AppController.setRegion(AppController.selectedPart, row.index, "sat", v) }
                            SSlider { width: parent.width; label: "Parlaklık"; from: 0.2; to: 2.5; neutral: 1; bipolar: true; value: row.modelData.val
                                      format: function(v) { return "%" + Math.round(v * 100) }
                                      onMoved: (v) => AppController.setRegion(AppController.selectedPart, row.index, "val", v) }
                            SSlider { width: parent.width; label: "Kontrast"; from: 0.4; to: 1.8; neutral: 1; bipolar: true; value: row.modelData.contrast
                                      format: function(v) { return "%" + Math.round(v * 100) }
                                      onMoved: (v) => AppController.setRegion(AppController.selectedPart, row.index, "contrast", v) }
                            SButton { small: true; kind: "subtle"; glyph: "↺"; text: "Bu bölgeyi sıfırla"; enabled: row.modelData.edited
                                      onClicked: AppController.resetRegion(AppController.selectedPart, row.index) }
                        }
                    }
                }
            }
        }

        // texture strip
        Rectangle {
            Layout.fillWidth: true
            height: 112; radius: 12; color: Theme.bg2; border.color: Theme.line
            Image {
                id: tex
                x: 10; y: 10; width: 92; height: 92
                source: "image://studio/part/" + AppController.selectedPart + "/" + AppController.revision
                sourceSize: Qt.size(256, 256); cache: false; fillMode: Image.PreserveAspectFit; asynchronous: true
            }
            Column {
                anchors.left: tex.right; anchors.leftMargin: 12; anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text { text: cp.part ? cp.part.label + " dokusu" : ""; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold }
                Text { text: cp.part ? cp.part.size + " · çözünürlük korunur" : ""; color: Theme.faint; font.pixelSize: 11 }
                SButton { small: true; kind: "subtle"; glyph: "↺"; text: "Parçayı sıfırla"; enabled: !!(cp.part && cp.part.edited); onClicked: AppController.resetPart(AppController.selectedPart) }
            }
        }
    }
}
