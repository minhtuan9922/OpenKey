import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "inputStyleSettings"
    required property var uiPalette
    property var controller: null
    readonly property bool ready: controller !== null && controller.ready
    readonly property bool busy: controller !== null && controller.busy
    readonly property string result: controller !== null ? controller.result : "offline"
    readonly property string runtimeStyle: controller !== null ? controller.runtimeStyle : ""
    readonly property string requestedStyle: picker.currentIndex === 1 ? "Direct" : "Preedit"
    readonly property string feedback: ({offline: "Đang offline; không đọc hoặc áp dụng cấu hình.",
        loading: "Đang đọc cấu hình từ Fcitx5…", loaded: "Đã đọc cấu hình runtime của addon OpenKey.",
        applying: "Đang áp dụng và đọc lại để xác nhận…",
        confirmed: "Đã xác nhận chế độ trong runtime. Xem riêng cấu hình đã lưu; chưa xác nhận lưu file thành công.",
        conflict: "Cấu hình đã đổi ở nơi khác. Nhấn Đọc lại trước khi thử tiếp.",
        unavailable: "Không kết nối được Fcitx5 hiện có. Giao diện không tự khởi động bộ gõ.",
        error: "Không đọc được cấu hình/schema tương thích. Chưa gửi thay đổi.",
        unconfirmed: "Chưa xác nhận kết quả; thay đổi có thể đã được áp dụng. Nhấn Đọc lại, không tự gửi lại.",
        invalid: "Giá trị chế độ không hợp lệ. Nhấn Đọc lại."})[result] || "Chưa xác định"
    function syncPicker() {
        if (controller !== null && controller.ready)
            picker.currentIndex = controller.runtimeStyle === "Direct" ? 1 : 0
    }
    Component.onCompleted: syncPicker()
    onControllerChanged: syncPicker()
    Connections {
        target: card.controller
        function onChanged() { card.syncPicker() }
    }
    padding: uiPalette.cardPadding
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: "Chế độ gõ — cấu hình runtime"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
            color: card.uiPalette.text
        }
        Label {
            text: card.runtimeStyle.length > 0 ?
                "Runtime: " + (card.runtimeStyle === "Direct" ? "Không gạch chân (Direct)" : "Có gạch chân (Preedit)") :
                "Runtime: chưa xác định"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
        SettingsComboBox {
            id: picker
            uiPalette: card.uiPalette
            objectName: "inputStylePicker"
            Layout.fillWidth: true
            model: ["Có gạch chân (Preedit)", "Không gạch chân (Direct)"]
            currentIndex: -1
            displayText: currentIndex < 0 ? "Chưa đọc được chế độ runtime" : currentText
            enabled: card.ready && !card.busy
            opacity: enabled ? 1 : 0.65
            Accessible.name: "Chế độ gõ"
            background: Rectangle {
                implicitHeight: 40
                radius: 5
                color: card.uiPalette.card
                border.width: picker.visualFocus ? 2 : 1
                border.color: picker.visualFocus ? card.uiPalette.accent : card.uiPalette.stroke
            }
        }
        RowLayout {
            spacing: 12
            Button {
                id: applyButton
                objectName: "applyInputStyle"
                text: "Áp dụng"
                implicitHeight: 36
                leftPadding: 18
                rightPadding: 18
                enabled: card.ready && !card.busy && picker.currentIndex >= 0 && card.requestedStyle !== card.runtimeStyle
                background: Rectangle {
                    radius: 5
                    color: applyButton.enabled ? card.uiPalette.accent : card.uiPalette.hover
                    border.width: applyButton.visualFocus ? 2 : 0
                    border.color: card.uiPalette.text
                }
                contentItem: Label {
                    text: applyButton.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: applyButton.enabled ? card.uiPalette.accentText : card.uiPalette.muted
                }
                onClicked: card.controller.apply(card.requestedStyle)
            }
            Button {
                id: reloadButton
                objectName: "reloadInputStyle"
                text: "Đọc lại"
                implicitHeight: 36
                leftPadding: 18
                rightPadding: 18
                enabled: card.controller !== null && !card.busy
                opacity: enabled ? 1 : 0.6
                background: Rectangle {
                    radius: 5
                    color: card.uiPalette.card
                    border.width: reloadButton.visualFocus ? 2 : 1
                    border.color: reloadButton.visualFocus ? card.uiPalette.accent : card.uiPalette.stroke
                }
                onClicked: card.controller.reload()
            }
            BusyIndicator { running: card.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
        }
        Label {
            objectName: "inputStyleFeedback"
            Layout.fillWidth: true
            text: card.feedback
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: "Chọn chế độ rồi nhấn Áp dụng khi đã gõ xong từ. Thay đổi áp dụng chung cho Telex/VNI; Direct có thể fallback Preedit tùy ứng dụng."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.smallSize
        }
    }
}
