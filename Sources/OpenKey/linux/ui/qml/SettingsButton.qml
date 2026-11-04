import QtQuick
import QtQuick.Controls.Basic

Button {
    id: button
    required property var uiPalette
    property bool primary: false
    Accessible.name: text
    implicitHeight: 36
    leftPadding: 18
    rightPadding: 18
    opacity: enabled || primary ? 1 : 0.6
    background: Rectangle {
        radius: 5
        color: button.primary ? (button.enabled ? button.uiPalette.accent : button.uiPalette.hover) : button.uiPalette.card
        border.width: button.visualFocus ? 2 : button.primary ? 0 : 1
        border.color: button.visualFocus ? (button.primary ? button.uiPalette.accentText : button.uiPalette.accent) : button.uiPalette.stroke
    }
    contentItem: Label {
        text: button.text
        font: button.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        color: button.enabled ? (button.primary ? button.uiPalette.accentText : button.uiPalette.text) : button.uiPalette.muted
    }
}
