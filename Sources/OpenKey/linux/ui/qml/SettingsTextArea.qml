import QtQuick
import QtQuick.Controls.Basic
TextArea {
    id: field
    required property var uiPalette
    color: uiPalette.text
    placeholderTextColor: uiPalette.muted
    selectionColor: uiPalette.accent
    selectedTextColor: uiPalette.accentText
    textFormat: TextEdit.PlainText
    wrapMode: TextEdit.Wrap
    selectByMouse: true
    padding: 10
    background: Rectangle {
        color: field.uiPalette.page
        radius: 5
        border.width: field.activeFocus ? 2 : 1
        border.color: field.activeFocus ? field.uiPalette.accent : field.uiPalette.stroke
    }
}
