import QtQuick
import SkinStudio

// Small vector icons drawn with Canvas so they look the same on macOS and Windows.
Canvas {
    id: c
    property string name: "colors"
    property color color: Theme.text
    implicitWidth: 22
    implicitHeight: 22
    onColorChanged: requestPaint()
    onNameChanged: requestPaint()
    onPaint: {
        var g = getContext("2d"); g.reset();
        var w = width, h = height, s = Math.min(w, h);
        g.strokeStyle = color; g.fillStyle = color; g.lineWidth = Math.max(1.6, s / 12); g.lineCap = "round"; g.lineJoin = "round";
        g.save(); g.translate((w - s) / 2, (h - s) / 2); g.scale(s / 24, s / 24);
        g.beginPath();
        switch (name) {
        case "colors":   // palette
            g.arc(12, 12, 9, 0, Math.PI * 2); g.stroke();
            g.beginPath(); g.arc(8, 9, 1.6, 0, 7); g.fill();
            g.beginPath(); g.arc(13, 7, 1.6, 0, 7); g.fill();
            g.beginPath(); g.arc(16.5, 11, 1.6, 0, 7); g.fill();
            g.beginPath(); g.arc(11, 16, 2.2, 0, 7); g.stroke(); break;
        case "effects":  // spark
            g.moveTo(12, 2); g.lineTo(14, 10); g.lineTo(22, 12); g.lineTo(14, 14); g.lineTo(12, 22); g.lineTo(10, 14); g.lineTo(2, 12); g.lineTo(10, 10); g.closePath(); g.stroke(); break;
        case "voice":    // mic
            g.roundedRect(9, 3, 6, 11, 3, 3); g.stroke();
            g.beginPath(); g.arc(12, 10, 6.5, 0.15 * Math.PI, 0.85 * Math.PI); g.stroke();
            g.beginPath(); g.moveTo(12, 16.5); g.lineTo(12, 21); g.moveTo(8.5, 21); g.lineTo(15.5, 21); g.stroke(); break;
        case "loadscreen": // portrait card
            g.roundedRect(6, 2.5, 12, 19, 2, 2); g.stroke();
            g.beginPath(); g.arc(12, 9, 2.5, 0, 7); g.stroke();
            g.beginPath(); g.moveTo(8, 18); g.quadraticCurveTo(12, 12, 16, 18); g.stroke(); break;
        case "package":  // box
            g.moveTo(3, 7.5); g.lineTo(12, 3); g.lineTo(21, 7.5); g.lineTo(21, 17); g.lineTo(12, 21.5); g.lineTo(3, 17); g.closePath(); g.stroke();
            g.beginPath(); g.moveTo(3, 7.5); g.lineTo(12, 12); g.lineTo(21, 7.5); g.moveTo(12, 12); g.lineTo(12, 21.5); g.stroke(); break;
        case "play": g.moveTo(7, 4.5); g.lineTo(19, 12); g.lineTo(7, 19.5); g.closePath(); g.fill(); break;
        case "pause": g.fillRect(6.5, 5, 4, 14); g.fillRect(13.5, 5, 4, 14); break;
        case "stop": g.fillRect(6, 6, 12, 12); break;
        case "rec": g.arc(12, 12, 6.5, 0, 7); g.fill(); break;
        case "file":  // folder/open
            g.moveTo(3, 6); g.lineTo(9, 6); g.lineTo(11, 8); g.lineTo(21, 8); g.lineTo(21, 19); g.lineTo(3, 19); g.closePath(); g.stroke(); break;
        case "mute":
            g.moveTo(4, 9); g.lineTo(8, 9); g.lineTo(13, 4.5); g.lineTo(13, 19.5); g.lineTo(8, 15); g.lineTo(4, 15); g.closePath(); g.stroke();
            g.beginPath(); g.moveTo(16, 9); g.lineTo(21, 15); g.moveTo(21, 9); g.lineTo(16, 15); g.stroke(); break;
        case "reset":
            g.arc(12, 12, 7.5, -0.6 * Math.PI, 1.1 * Math.PI); g.stroke();
            g.beginPath(); g.moveTo(9.5, 2.5); g.lineTo(9, 6); g.lineTo(12.5, 6.8); g.stroke(); break;
        case "upload":
            g.moveTo(12, 16); g.lineTo(12, 4); g.moveTo(7, 9); g.lineTo(12, 4); g.lineTo(17, 9); g.stroke();
            g.beginPath(); g.moveTo(4, 15); g.lineTo(4, 20); g.lineTo(20, 20); g.lineTo(20, 15); g.stroke(); break;
        case "check": g.moveTo(5, 12.5); g.lineTo(10, 17.5); g.lineTo(19, 7); g.stroke(); break;
        case "close": g.moveTo(6, 6); g.lineTo(18, 18); g.moveTo(18, 6); g.lineTo(6, 18); g.stroke(); break;
        case "chevron": g.moveTo(7, 9.5); g.lineTo(12, 14.5); g.lineTo(17, 9.5); g.stroke(); break;
        case "scissors":
            g.arc(7, 17, 3, 0, 7); g.stroke(); g.beginPath(); g.arc(17, 17, 3, 0, 7); g.stroke();
            g.beginPath(); g.moveTo(9, 15); g.lineTo(18, 3); g.moveTo(15, 15); g.lineTo(6, 3); g.stroke(); break;
        case "save":
            g.moveTo(12, 4); g.lineTo(12, 15); g.moveTo(7, 10.5); g.lineTo(12, 15.5); g.lineTo(17, 10.5); g.stroke();
            g.beginPath(); g.moveTo(4, 15); g.lineTo(4, 20); g.lineTo(20, 20); g.lineTo(20, 15); g.stroke(); break;
        case "folder":
            g.moveTo(3, 6); g.lineTo(9, 6); g.lineTo(11, 8); g.lineTo(21, 8); g.lineTo(21, 19); g.lineTo(3, 19); g.closePath(); g.stroke(); break;
        }
        g.restore();
    }
}
