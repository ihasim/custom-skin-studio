import QtQuick
import QtQuick.Controls.Basic
import SkinStudio

// Labelled slider. Double-click resets to `neutral`. `bipolar` fills from the neutral value.
Item {
    id: r
    property string label
    property real from: 0
    property real to: 1
    property real value: 0
    property real neutral: 0
    property real stepSize: 0
    property bool bipolar: false
    property var format: function(v) { return v.toFixed(2) }
    signal moved(real value)
    implicitHeight: 44
    implicitWidth: 240

    Text { id: lab; text: r.label; color: Theme.muted; font.family: Theme.font; font.pixelSize: 12; anchors.left: parent.left; anchors.top: parent.top }
    Text {
        text: r.format(r.value); anchors.right: parent.right; anchors.top: parent.top
        color: Math.abs(r.value - r.neutral) > 1e-6 ? Theme.text : Theme.faint
        font.family: Theme.font; font.pixelSize: 12; font.features: { "tnum": 1 }
    }
    Slider {
        id: s
        anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
        height: 24
        from: r.from; to: r.to; stepSize: r.stepSize
        value: r.value
        onMoved: r.moved(value)
        padding: 0
        background: Item {
            x: s.leftPadding; y: s.topPadding + s.availableHeight / 2 - 2; width: s.availableWidth; height: 4
            Rectangle { anchors.fill: parent; radius: 2; color: Theme.line }
            Rectangle {
                readonly property real n: (r.neutral - r.from) / (r.to - r.from)
                readonly property real a: r.bipolar ? Math.min(n, s.visualPosition) : 0
                readonly property real b: r.bipolar ? Math.max(n, s.visualPosition) : s.visualPosition
                x: a * parent.width; width: (b - a) * parent.width; height: 4; radius: 2; color: Theme.accent
            }
        }
        handle: Rectangle {
            x: s.leftPadding + s.visualPosition * (s.availableWidth - width); y: s.topPadding + s.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8; color: s.pressed ? Theme.accent : Theme.text; border.color: Theme.bg; border.width: 2
        }
        TapHandler { acceptedButtons: Qt.LeftButton; onDoubleTapped: r.moved(r.neutral) }
    }
}
