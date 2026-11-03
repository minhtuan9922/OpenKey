import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    required property var uiPalette
    property string heading
    property string description
    property string badge: ""
    padding: uiPalette.cardPadding
    background: Rectangle {
        color: card.uiPalette.card
        radius: card.uiPalette.radius
        border.color: card.uiPalette.stroke
    }
    contentItem: ColumnLayout {
        spacing: 12
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: card.heading
                color: card.uiPalette.text
                font.pixelSize: card.uiPalette.headingSize; font.weight: Font.DemiBold
                wrapMode: Text.Wrap
            }
            Rectangle {
                visible: card.badge.length > 0
                implicitWidth: badgeText.implicitWidth + 16
                implicitHeight: 26
                radius: 5
                color: card.uiPalette.selected
                Label {
                    id: badgeText
                    anchors.centerIn: parent
                    text: card.badge
                    font.pixelSize: 11
                    color: card.uiPalette.accent
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: card.description
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
            lineHeight: 1.35
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
        }
    }
}
