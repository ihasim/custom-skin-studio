import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Dialogs
import QtQuick.Layouts
import SkinStudio

Item {
    id: pp
    signal saveRequested()
    FileDialog { id: coverDlg; nameFilters: ["Görseller (*.png *.jpg *.jpeg *.webp)"]; onAccepted: AppController.setCover(selectedFile) }
    FolderDialog { id: gameDlg; onAccepted: AppController.setGameDir(selectedFolder) }

    ScrollView {
        anchors.fill: parent; anchors.margins: 18
        contentWidth: availableWidth
        clip: true
        Column {
            width: parent.width
            spacing: 14
            Column {
                spacing: 4
                Text { text: "Paket"; color: Theme.text; font.pixelSize: 20; font.weight: Font.Bold }
                Text { text: "ROSE'da görünen bilgiler ve yeni sürüm."; color: Theme.muted; font.pixelSize: 12 }
            }
            SField { width: parent.width; label: "Ad"; text: AppController.pkgName; onEdited: (t) => AppController.pkgName = t }
            Row {
                width: parent.width; spacing: 10
                SField { width: (parent.width - 10) * 0.62; label: "Yapan"; text: AppController.pkgAuthor; onEdited: (t) => AppController.pkgAuthor = t }
                SField { width: (parent.width - 10) * 0.38; label: "Sürüm"; text: AppController.pkgVersion; onEdited: (t) => AppController.pkgVersion = t }
            }
            SField { width: parent.width; label: "Açıklama"; multiline: true; text: AppController.pkgDescription; onEdited: (t) => AppController.pkgDescription = t }

            Text { text: "Kapak görseli"; color: Theme.muted; font.pixelSize: 12 }
            Row {
                spacing: 12
                Rectangle { width: 150; height: 150; radius: 10; color: Theme.bg2; border.color: Theme.line; clip: true
                    Image { anchors.fill: parent; anchors.margins: 4; source: "image://studio/cover/" + AppController.revision; cache: false; fillMode: Image.PreserveAspectFit } }
                Column { spacing: 8
                    SButton { small: true; text: "Dosya seç"; onClicked: coverDlg.open() }
                    SButton { small: true; text: "Yükleme ekranından"; onClicked: AppController.coverFromLoadscreen() } }
            }

            Rectangle {
                width: parent.width; height: sumCol.implicitHeight + 24; radius: 12; color: Theme.bg2; border.color: Theme.line
                Column {
                    id: sumCol; x: 12; y: 12; width: parent.width - 24; spacing: 6
                    Text { text: "Bu sürümde değişenler"; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold }
                    Text { width: parent.width; wrapMode: Text.Wrap; color: AppController.changeSummary ? Theme.text : Theme.faint; font.pixelSize: 12; lineHeight: 1.3
                           text: AppController.changeSummary || "Henüz değişiklik yok." }
                }
            }
            SButton { kind: "primary"; glyph: "⇩"; text: "Yeni sürüm olarak kaydet"; enabled: AppController.dirty; onClicked: pp.saveRequested() }
            Text { width: parent.width; wrapMode: Text.Wrap; color: Theme.faint; font.pixelSize: 11
                   text: "Açılan paketin üzerine yazılmaz; yeni bir .fantome oluşur. Oyuna yalnızca görsel ve ses değişikliği girer." }

            Rectangle { width: parent.width; height: 1; color: Theme.line }
            Text { text: "Oyun klasörü"; color: Theme.muted; font.pixelSize: 12 }
            Text { width: parent.width; wrapMode: Text.WrapAnywhere; text: AppController.gameDir || "seçilmedi"; color: Theme.text; font.pixelSize: 12 }
            Text { width: parent.width; wrapMode: Text.Wrap; color: Theme.faint; font.pixelSize: 11
                   text: "İskelet, animasyonlar ve paketin içinde olmayan dokular buradan okunur. Windows'ta genelde C:\\Riot Games\\League of Legends." }
            SButton { small: true; glyph: ""; text: "Klasör seç"; onClicked: gameDlg.open() }
        }
    }
}
