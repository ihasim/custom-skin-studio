import QtQuick
import QtQuick.Controls.Basic
import SkinStudio

Column {
    id: f
    property string label
    property alias text: field.text
    property bool multiline: false
    property alias placeholder: field.placeholderText
    signal edited(string text)
    spacing: 6
    Text { text: f.label; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12 }
    TextField {
        id: field
        width: f.width
        visible: !f.multiline
        color: Theme.text; font.family: Theme.font; font.pixelSize: 13
        placeholderTextColor: Theme.faint
        selectByMouse: true
        onTextEdited: f.edited(text)
        background: Rectangle { radius: 8; color: Theme.bg2; border.color: field.activeFocus ? Theme.accentDim : Theme.line }
    }
    TextArea {
        id: area
        width: f.width
        visible: f.multiline
        text: field.text
        wrapMode: TextEdit.Wrap
        color: Theme.text; font.family: Theme.font; font.pixelSize: 13
        selectByMouse: true
        onTextChanged: if (activeFocus) { field.text = text; f.edited(text) }
        background: Rectangle { implicitHeight: 90; radius: 8; color: Theme.bg2; border.color: area.activeFocus ? Theme.accentDim : Theme.line }
    }
}
