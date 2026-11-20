import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Item {
    id: lp
    signal saveRequested()
    signal cropRequested(url file)
    // test hook: behaves exactly like choosing a file in the dialog
    function simulatePick(file) { imgDlg.selectedFile = file; imgDlg.accepted() }
    FileDialog {
        id: imgDlg
        title: "Yükleme ekranı görseli seç"
        nameFilters: ["Görseller (*.png *.jpg *.jpeg *.webp *.bmp)", "Tüm dosyalar (*)"]
        onAccepted: lp.cropRequested(selectedFile)
    }
    ColumnLayout {
        anchors.fill: parent; anchors.margins: 18
        spacing: 14
        Column {
            spacing: 4
            Text { text: "Yükleme ekranı"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
            Text { width: lp.width - 36; wrapMode: Text.Wrap; color: Theme.muted; font.pixelSize: 12
                   text: "Oyun açılırken oyuncu kartında görünen görsel. Bir görsel seç, kırpılacak alanı ayarla." }
        }
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true
            radius: 14; color: Theme.bg2; border.color: Theme.line
            DropArea { anchors.fill: parent; onDropped: (d) => { if (d.hasUrls) lp.cropRequested(d.urls[0]) } }
            Image {
                id: cur
                anchors.centerIn: parent
                height: Math.min(parent.height - 40, (parent.width - 40) * AppController.loadscreenSize.height / AppController.loadscreenSize.width)
                width: height * AppController.loadscreenSize.width / AppController.loadscreenSize.height
                source: AppController.hasLoadscreen || AppController.loadscreenChanged ? "image://studio/loadscreen/" + AppController.revision : ""
                cache: false; fillMode: Image.PreserveAspectFit
                Rectangle { anchors.fill: parent; color: "transparent"; border.color: Theme.gold; border.width: 2; radius: 4 }
                Rectangle {
                    anchors.bottom: parent.bottom; width: parent.width; height: 46
                    gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "#cc000000" } }
                    Text { anchors.centerIn: parent; text: AppController.pkgName || AppController.champion; color: "white"; font.pixelSize: 13; font.weight: Font.DemiBold }
                }
            }
            Text { anchors.centerIn: parent; visible: cur.source == ""; text: "Görsel bulunamadı"; color: Theme.faint }
        }
        Text { text: AppController.loadscreenSize.width + "×" + AppController.loadscreenSize.height + " piksel, oyunun kendi boyutu"
               + (AppController.loadscreenChanged ? " · değişti" : ""); color: AppController.loadscreenChanged ? Theme.accent : Theme.faint; font.pixelSize: 11 }
        RowLayout {
            spacing: 8
            SButton { kind: "primary"; glyph: "⇪"; text: "Görsel seç"; onClicked: imgDlg.open() }
            SButton { text: "Orijinale dön"; enabled: AppController.loadscreenChanged; onClicked: AppController.resetLoadscreen() }
        }
        SButton { kind: "subtle"; text: "Paket kapak görseli olarak da kullan"; onClicked: AppController.coverFromLoadscreen() }
        Text { Layout.fillWidth: true; wrapMode: Text.Wrap; color: Theme.faint; font.pixelSize: 11
               text: "Oyunda görünüp görünmediği henüz doğrulanmadı: ROSE ile kurduktan sonra yükleme ekranında kontrol et." }
    }
}
