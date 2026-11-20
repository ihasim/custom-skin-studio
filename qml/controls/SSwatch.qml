import QtQuick
import SkinStudio

Rectangle {
    id: sw
    property color swatch: "gray"
    property bool selected: false
    signal clicked()
    implicitWidth: 24; implicitHeight: 24
    radius: width / 2
    color: swatch
    border.width: selected ? 2 : 1
    border.color: selected ? Theme.text : Qt.rgba(1, 1, 1, 0.16)
    Rectangle { visible: sw.selected; anchors.centerIn: parent; width: parent.width + 6; height: width; radius: width / 2; color: "transparent"; border.color: Theme.accent; border.width: 2 }
    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: sw.clicked() }
}
