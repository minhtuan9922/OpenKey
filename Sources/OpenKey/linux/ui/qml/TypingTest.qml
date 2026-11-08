import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "typingTestCard"
    required property var uiPalette
    readonly property string sampleKeys: sample.currentIndex === 1 ?
        "co6ng5 hoa2 xa4 ho6i5 chu3 nghia4" : "coongj hoaf xax hooij chur nghiax"
    padding: 20
    background: Rectangle {
        color: card.uiPalette.card
        radius: card.uiPalette.radius
        border.color: card.uiPalette.stroke
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 12
        Label {
            text: "Thử gõ tiếng Việt"
            color: card.uiPalette.text
            font.pixelSize: 18
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: "Bật OpenKey bằng phím tắt hoặc menu Fcitx5, rồi gõ vào ô bên dưới. Chọn mẫu chỉ đổi hướng dẫn; không đổi bộ gõ đang dùng."
            color: card.uiPalette.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        SettingsComboBox {
            id: sample
            objectName: "typingTestSample"
            uiPalette: card.uiPalette
            Layout.fillWidth: true
            model: ["Mẫu Telex", "Mẫu VNI", "Mẫu Simple Telex 1", "Mẫu Simple Telex 2"]
            Accessible.name: "Kiểu gõ của mẫu thử"
        }
        Label {
            objectName: "typingTestInstructions"
            text: "Gõ: " + card.sampleKeys +
                "\nKết quả tham chiếu: cộng hoà xã hội chủ nghĩa\nĐặt dấu kiểu cũ có thể cho ‘hòa’; cả hai cách đều phù hợp để thử."
            color: card.uiPalette.text
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            clip: true
            TextArea {
                id: input
                objectName: "typingTestInput"
                placeholderText: "Gõ thử ở đây…"
                Accessible.name: "Ô thử gõ tiếng Việt"
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                selectByMouse: true
                color: card.uiPalette.text
                placeholderTextColor: card.uiPalette.muted
                selectionColor: card.uiPalette.accent
                selectedTextColor: card.uiPalette.accentText
                padding: 12
                background: Rectangle {
                    color: card.uiPalette.page
                    radius: 5
                    border.color: input.activeFocus ? card.uiPalette.accent : card.uiPalette.stroke
                    border.width: input.activeFocus ? 2 : 1
                }
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "focusTypingTest"
                uiPalette: card.uiPalette
                text: "Bắt đầu thử"
                primary: true
                onClicked: input.forceActiveFocus(Qt.TabFocusReason)
            }
            SettingsButton {
                objectName: "clearTypingTest"
                uiPalette: card.uiPalette
                text: "Xóa nội dung thử"
                enabled: input.length > 0 || input.inputMethodComposing
                onClicked: {
                    Qt.inputMethod.reset()
                    input.clear()
                    input.forceActiveFocus(Qt.TabFocusReason)
                }
            }
        }
        Label {
            text: "Thử thêm Backspace, Enter, Ctrl+A, hoàn tác và chuyển sang trang khác rồi quay lại. Nội dung chỉ giữ trong cửa sổ, không được lưu vào cấu hình.\n\nKết quả ở ô Qt này không xác nhận khả năng gõ trong Chrome, VS Code hoặc terminal."
            color: card.uiPalette.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
    }
}
