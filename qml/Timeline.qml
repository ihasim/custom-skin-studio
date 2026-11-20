import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import SkinStudio

Rectangle {
    id: tl
    height: 92
    color: Theme.bg2
    Rectangle { width: parent.width; height: 1; color: Theme.line }

    RowLayout {
        anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 14; anchors.topMargin: 10; anchors.bottomMargin: 10
        spacing: 12

        Rectangle {
            width: 44; height: 44; radius: 22
            color: AppController.animation >= 0 ? Theme.accent : Theme.panel2
            SIcon { anchors.centerIn: parent; name: AppController.playing ? "pause" : "play"; color: Theme.accentText; width: 18; height: 18 }
            MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: AppController.playing = !AppController.playing }
        }

        ComboBox {
            id: anims
            Layout.preferredWidth: 210
            model: AppController.animations
            textRole: "label"
            currentIndex: AppController.animation
            enabled: count > 0
            displayText: count > 0 ? (currentIndex >= 0 ? AppController.animations[currentIndex].label : "Bekleme duruşu") : "Animasyon yok"
            onActivated: (i) => { AppController.animation = i; AppController.playing = true }
            font.pixelSize: 13
            background: Rectangle { implicitHeight: 38; radius: 8; color: Theme.panel; border.color: Theme.line }
            contentItem: Text { leftPadding: 12; text: anims.displayText; color: Theme.text; font: anims.font; verticalAlignment: Text.AlignVCenter; elide: Text.ElideRight }
            indicator: SIcon { name: "chevron"; color: Theme.muted; width: 14; height: 14; x: anims.width - width - 10; y: (anims.height - height) / 2 }
            delegate: ItemDelegate {
                width: anims.width
                highlighted: anims.highlightedIndex === index
                contentItem: Row { spacing: 8
                    Text { text: modelData.label; color: Theme.text; font.pixelSize: 13 }
                    Text { text: modelData.clip; color: Theme.faint; font.pixelSize: 11; anchors.baseline: parent.children[0].baseline } }
                background: Rectangle { color: highlighted ? Theme.panel2 : Theme.panel }
            }
            popup.height: Math.min(460, contentItemHeight())
            function contentItemHeight() { return count * 36 + 8 }
            popup.background: Rectangle { color: Theme.panel; border.color: Theme.line; radius: 8 }
        }

        Item {
            Layout.fillWidth: true; Layout.fillHeight: true
            // voice lines bound to this animation
            Row {
                anchors.left: parent.left; anchors.top: parent.top
                spacing: 6
                Text { text: AppController.voiceMarkers.length ? "Bu animasyonda çalan sesler:" : ""; color: Theme.faint; font.pixelSize: 11; anchors.verticalCenter: parent.verticalCenter }
                Repeater {
                    model: AppController.voiceMarkers
                    delegate: Rectangle {
                        height: 20; radius: 10; width: mt.implicitWidth + 16
                        color: modelData.mode === "replace" ? Qt.rgba(0.18, 0.83, 0.81, 0.15) : modelData.mode === "mute" ? Qt.rgba(1, 0.4, 0.4, 0.12) : Theme.panel
                        border.color: modelData.mode === "replace" ? Theme.accentDim : Theme.line
                        Text { id: mt; anchors.centerIn: parent; text: (modelData.mode === "mute" ? "🔇 " : "♪ ") + modelData.label; color: Theme.muted; font.pixelSize: 11 }
                    }
                }
            }
            Slider {
                id: scrub
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 26
                from: 0; to: Math.max(0.01, AppController.animDuration)
                value: AppController.animTime
                enabled: AppController.animation >= 0
                onMoved: { AppController.playing = false; AppController.animTime = value }
                padding: 0
                background: Rectangle { x: 0; y: scrub.height / 2 - 2; width: scrub.width; height: 4; radius: 2; color: Theme.line
                    Rectangle { width: scrub.visualPosition * parent.width; height: 4; radius: 2; color: Theme.accentDim } }
                handle: Rectangle { x: scrub.visualPosition * (scrub.width - width); y: scrub.height / 2 - 7; width: 14; height: 14; radius: 7; color: Theme.text }
            }
        }
        Text {
            text: AppController.animTime.toFixed(2) + " / " + AppController.animDuration.toFixed(2) + " sn"
            color: Theme.muted; font.pixelSize: 12; font.features: { "tnum": 1 }
            Layout.preferredWidth: 100
        }
    }
}
