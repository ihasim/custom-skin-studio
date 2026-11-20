import QtQuick
import QtQuick.Controls.Basic
import SkinStudio

Switch {
    id: t
    font.family: Theme.font
    font.pixelSize: 13
    indicator: Rectangle {
        implicitWidth: 36; implicitHeight: 20; radius: 10
        x: t.leftPadding; y: parent.height / 2 - height / 2
        color: t.checked ? Theme.accent : Theme.line
        Rectangle { x: t.checked ? parent.width - width - 2 : 2; y: 2; width: 16; height: 16; radius: 8; color: t.checked ? Theme.accentText : Theme.muted
                    Behavior on x { NumberAnimation { duration: 120 } } }
    }
    contentItem: Text { text: t.text; font: t.font; color: Theme.text; verticalAlignment: Text.AlignVCenter; leftPadding: t.indicator.width + 8 }
}
