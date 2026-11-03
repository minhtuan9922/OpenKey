import QtQuick

QtObject {
    objectName: "uiPalette"
    property bool dark: false
    readonly property color page: dark ? "#202020" : "#f3f3f3"
    readonly property color sidebar: dark ? "#181818" : "#eeeeee"
    readonly property color card: dark ? "#2b2b2b" : "#ffffff"
    readonly property color stroke: dark ? "#414141" : "#dedede"
    readonly property color text: dark ? "#f3f3f3" : "#1b1b1b"
    readonly property color muted: dark ? "#bdbdbd" : "#606060"
    readonly property color accent: dark ? "#60cdff" : "#0067c0"
    readonly property color accentText: dark ? "#10202a" : "#ffffff"
    readonly property color selected: dark ? "#293e49" : "#dfeaf4"
    readonly property color hover: dark ? "#303030" : "#e1e1e1"
    readonly property int pagePadding: 24
    readonly property int cardPadding: 20
    readonly property int sectionSpacing: 20
    readonly property int radius: 8
    readonly property int titleSize: 28
    readonly property int headingSize: 16
    readonly property int bodySize: 14
    readonly property int smallSize: 12
}
