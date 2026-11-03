import QtQuick
import QtQuick.Controls.Basic

// String-list picker. Pair popup text/background explicitly: Basic's default
// ItemDelegate can combine highlightedText with a platform light background.
ComboBox {
    id: combo
    required property var uiPalette
    delegate: ItemDelegate {
        id: option
        required property var modelData
        required property int index
        objectName: combo.objectName + "-option-" + index
        width: ListView.view ? ListView.view.width : combo.width
        text: modelData
        highlighted: combo.highlightedIndex === index
        hoverEnabled: true
        font.weight: combo.currentIndex === index ? Font.DemiBold : Font.Normal
        contentItem: Label {
            objectName: combo.objectName + "-option-text-" + option.index
            text: option.text
            font: option.font
            color: combo.uiPalette.text
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            objectName: combo.objectName + "-option-background-" + option.index
            implicitHeight: 40
            color: option.highlighted || combo.currentIndex === option.index ? combo.uiPalette.selected :
                option.hovered ? combo.uiPalette.hover : combo.uiPalette.card
            border.width: option.visualFocus ? 1 : 0
            border.color: combo.uiPalette.accent
            Rectangle {
                width: 3
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.margins: 4
                color: combo.uiPalette.accent
                visible: combo.currentIndex === option.index
            }
        }
    }
}
