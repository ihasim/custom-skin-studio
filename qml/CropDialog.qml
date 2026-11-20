import QtQuick
import QtQuick.Controls.Basic
import SkinStudio

// Full-window crop: the frame keeps the loading-screen aspect; drag it to move, drag the corner to resize.
Rectangle {
    id: cd
    visible: false
    color: Theme.bg
    property url source
    property size imgSize: Qt.size(1, 1)
    readonly property real aspect: AppController.loadscreenSize.width / AppController.loadscreenSize.height
    // crop rect in normalized image coordinates
    property real cx: 0; property real cy: 0; property real cw: 1; property real ch: 1

    function openWith(url) {
        source = url
        imgSize = AppController.imageSize(url)
        if (imgSize.width <= 0) { AppController.clearError(); return }
        // largest centered rect with the right aspect
        var ia = imgSize.width / imgSize.height
        if (ia > aspect) { ch = 1; cw = aspect / ia } else { cw = 1; ch = ia / aspect }
        cx = (1 - cw) / 2; cy = (1 - ch) / 2
        visible = true
    }

    MouseArea { anchors.fill: parent; hoverEnabled: true; onWheel: (w) => {} }   // block the editor below

    Text { id: title; x: 28; y: 22; text: "Kırpılacak alanı ayarla"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
    Text { anchors.left: title.left; anchors.top: title.bottom; anchors.topMargin: 4; color: Theme.muted; font.pixelSize: 12
           text: "Çerçeveyi sürükle, sağ alt köşeden büyüt ya da küçült. Oran oyunun yükleme ekranıyla aynı (" + AppController.loadscreenSize.width + "×" + AppController.loadscreenSize.height + ")." }

    Item {
        id: stage
        anchors.fill: parent; anchors.topMargin: 90; anchors.bottomMargin: 80; anchors.leftMargin: 28; anchors.rightMargin: 300
        Image {
            id: img
            anchors.centerIn: parent
            source: cd.visible ? "image://studio/lsource/" + AppController.revision : ""
            cache: false
            fillMode: Image.PreserveAspectFit
            width: Math.min(stage.width, stage.height * cd.imgSize.width / cd.imgSize.height)
            height: width * cd.imgSize.height / cd.imgSize.width
            // dim outside
            Rectangle { x: 0; y: 0; width: parent.width; height: frame.y; color: "#aa000000" }
            Rectangle { x: 0; y: frame.y + frame.height; width: parent.width; height: parent.height - y; color: "#aa000000" }
            Rectangle { x: 0; y: frame.y; width: frame.x; height: frame.height; color: "#aa000000" }
            Rectangle { x: frame.x + frame.width; y: frame.y; width: parent.width - x; height: frame.height; color: "#aa000000" }
            Rectangle {
                id: frame
                x: cd.cx * img.width; y: cd.cy * img.height; width: cd.cw * img.width; height: cd.ch * img.height
                color: "transparent"; border.color: Theme.gold; border.width: 2
                MouseArea {
                    anchors.fill: parent; cursorShape: Qt.SizeAllCursor
                    property point start; property real sx; property real sy
                    onPressed: (m) => { start = mapToItem(img, m.x, m.y); sx = cd.cx; sy = cd.cy }
                    onPositionChanged: (m) => {
                        var p = mapToItem(img, m.x, m.y)
                        cd.cx = Math.max(0, Math.min(1 - cd.cw, sx + (p.x - start.x) / img.width))
                        cd.cy = Math.max(0, Math.min(1 - cd.ch, sy + (p.y - start.y) / img.height))
                    }
                }
                Rectangle {
                    width: 18; height: 18; radius: 4; color: Theme.gold
                    anchors.right: parent.right; anchors.bottom: parent.bottom; anchors.margins: -9
                    MouseArea {
                        anchors.fill: parent; anchors.margins: -6; cursorShape: Qt.SizeFDiagCursor
                        onPositionChanged: (m) => {
                            var p = mapToItem(img, m.x, m.y)
                            var wpx = Math.max(40, p.x - frame.x)
                            var nw = wpx / img.width
                            var nh = nw * img.width / cd.aspect / img.height
                            if (cd.cx + nw > 1) { nw = 1 - cd.cx; nh = nw * img.width / cd.aspect / img.height }
                            if (cd.cy + nh > 1) { nh = 1 - cd.cy; nw = nh * img.height * cd.aspect / img.width }
                            cd.cw = nw; cd.ch = nh
                        }
                    }
                }
            }
        }
    }

    // live preview of the card
    Column {
        anchors.right: parent.right; anchors.rightMargin: 28; anchors.top: stage.top
        spacing: 10
        Text { text: "Önizleme"; color: Theme.muted; font.pixelSize: 12 }
        Rectangle {
            width: 240; height: width / cd.aspect; clip: true; color: "black"; border.color: Theme.gold; border.width: 2
            Image {
                source: img.source; cache: false
                width: parent.width / cd.cw; height: parent.height / cd.ch
                x: -cd.cx * width; y: -cd.cy * height
            }
        }
    }

    Row {
        anchors.right: parent.right; anchors.rightMargin: 28; anchors.bottom: parent.bottom; anchors.bottomMargin: 24
        spacing: 10
        SButton { text: "Vazgeç"; onClicked: cd.visible = false }
        SButton { kind: "primary"; text: "Uygula"; onClicked: { AppController.setLoadscreen("", cd.cx, cd.cy, cd.cw, cd.ch); cd.visible = false } }
    }
}
