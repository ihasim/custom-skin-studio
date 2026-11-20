import QtQuick
import QtQuick.Controls.Basic
import SkinStudio

Button {
    id: b
    property string kind: "ghost"      // primary | ghost | danger | subtle
    property bool small: false
    property string glyph: ""
    implicitHeight: small ? 28 : 36
    leftPadding: small ? 10 : 14
    rightPadding: small ? 10 : 14
    font.family: Theme.font
    font.pixelSize: small ? 12 : 13
    font.weight: Font.DemiBold
    hoverEnabled: true
    contentItem: Row {
        spacing: 6
        Text {
            visible: b.glyph !== ""
            text: b.glyph
            color: label.color
            font.pixelSize: b.small ? 12 : 14
            anchors.verticalCenter: parent.verticalCenter
        }
        Text {
            id: label
            text: b.text
            font: b.font
            anchors.verticalCenter: parent.verticalCenter
            color: !b.enabled ? Theme.faint
                 : b.kind === "primary" ? Theme.accentText
                 : b.kind === "danger" ? Theme.danger
                 : b.checked ? Theme.accent : Theme.text
        }
    }
    background: Rectangle {
        radius: 8
        color: !b.enabled ? (b.kind === "primary" ? Theme.panel2 : "transparent")
             : b.kind === "primary" ? (b.down ? Qt.darker(Theme.accent, 1.2) : b.hovered ? Qt.lighter(Theme.accent, 1.1) : Theme.accent)
             : b.kind === "subtle" ? (b.hovered ? Theme.panel2 : "transparent")
             : (b.down ? Theme.lineStrong : b.hovered || b.checked ? Theme.panel2 : Theme.bg2)
        border.width: b.kind === "primary" || b.kind === "subtle" ? 0 : 1
        border.color: b.checked ? Theme.accentDim : Theme.line
    }
}
