pragma Singleton
import QtQuick

QtObject {
    readonly property color bg: "#0d1016"
    readonly property color bg2: "#121620"
    readonly property color panel: "#161b25"
    readonly property color panel2: "#1c2230"
    readonly property color line: "#262e3d"
    readonly property color lineStrong: "#344055"
    readonly property color text: "#e8ebf2"
    readonly property color muted: "#8c96aa"
    readonly property color faint: "#5c6578"
    readonly property color accent: "#2fd3cf"
    readonly property color accentDim: "#1d6f70"
    readonly property color accentText: "#071415"
    readonly property color gold: "#e3b450"
    readonly property color danger: "#f06464"
    readonly property color ok: "#4fd18b"
    readonly property color warn: "#f0a94a"
    readonly property int radius: 10
    readonly property int gap: 12
    readonly property string font: Qt.platform.os === "windows" ? "Segoe UI" : "Helvetica Neue"
    readonly property var palette: ["#e63946", "#f4a261", "#e9c46a", "#2a9d8f", "#2fd3cf", "#3a86ff",
                                    "#4361ee", "#7b2ff7", "#c77dff", "#ff4d9d", "#f1f1f1", "#2b2b2b"]
    function groupTitle(g) {
        return ({"P": "Pasif", "Q": "Q", "W": "W", "E": "E", "R": "R (ulti)", "A": "Saldırı",
                 "M": "Hareket", "D": "Ölüm ve doğma", "S": "Şaka, kışkırtma, dans", "K": "Rakiplere özel", "O": "Diğer"})[g] || g
    }
}
