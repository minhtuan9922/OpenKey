import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Button {
    id: button
    required property var uiPalette
    property string symbol: "home"
    property bool selected: false
    property bool expanded: true
    implicitHeight: 44
    leftPadding: 14
    rightPadding: 12
    hoverEnabled: true
    Accessible.name: text
    Accessible.description: selected ? "Trang đang chọn" : "Mở trang " + text
    Accessible.checkable: true
    Accessible.checked: selected
    ToolTip.visible: (hovered || visualFocus) && !expanded
    ToolTip.delay: 300
    ToolTip.text: text
    background: Rectangle {
        radius: button.uiPalette.radius
        color: button.selected ? button.uiPalette.selected : button.hovered ? button.uiPalette.hover : "transparent"
        border.width: button.activeFocus ? 2 : 0
        border.color: button.uiPalette.accent
        Rectangle {
            width: 3; height: 18; radius: 1
            anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter
            color: button.uiPalette.accent
            visible: button.selected
        }
    }
    contentItem: RowLayout {
        spacing: 12
        NavIcon {
            Layout.preferredWidth: 20; Layout.preferredHeight: 20
            symbol: button.symbol
            ink: button.selected ? button.uiPalette.accent : button.uiPalette.text
        }
        Label {
            Layout.fillWidth: true
            visible: button.expanded
            text: button.text
            color: button.uiPalette.text
            font.pixelSize: button.uiPalette.bodySize
            elide: Text.ElideRight
        }
    }
}
