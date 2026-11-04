import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "diagnosticsSettings"
    required property var uiPalette
    property var controller: null
    readonly property bool ready: controller !== null && controller.ready
    readonly property bool busy: controller !== null && controller.busy
    readonly property string runtimeDiagnostics: controller !== null ? controller.runtimeDiagnostics : ""
    readonly property string requestedValue: picker.currentIndex === 1 ? "True" : "False"
    ConfigFeedback { id: feedback; result: card.controller !== null ? card.controller.result : "offline" }
    function syncPicker() {
        if (!picker) return
        if (controller !== null && controller.ready)
            picker.currentIndex = controller.runtimeDiagnostics === "True" ? 1 : 0
        else if (controller === null || !controller.busy)
            picker.currentIndex = -1
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
            Layout.fillWidth: true
            text: "Chẩn đoán Direct"
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
            color: card.uiPalette.text
        }
        Label {
            Layout.fillWidth: true
            text: "Chỉ bật khi cần điều tra lỗi chế độ không gạch chân. Log của addon ghi lý do xử lý, app/frontend và capability; không ghi nội dung gõ. Giao diện không đọc hoặc tải log lên."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
        Label {
            objectName: "runtimeDiagnosticsLabel"
            Layout.fillWidth: true
            text: "Runtime: " + (card.runtimeDiagnostics === "True" ? "Bật" : card.runtimeDiagnostics === "False" ? "Tắt" : "Chưa xác định")
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.bodySize
        }
        SettingsComboBox {
            id: picker
            objectName: "diagnosticsPicker"
            Layout.fillWidth: true
            uiPalette: card.uiPalette
            model: ["Tắt chẩn đoán Direct", "Bật chẩn đoán Direct"]
            currentIndex: -1
            displayText: currentIndex < 0 ? "Chưa đọc được thiết lập runtime" : currentText
            enabled: card.ready && !card.busy
            opacity: enabled ? 1 : 0.65
            Accessible.name: "Chẩn đoán Direct"
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
            SettingsButton {
                objectName: "applyDiagnostics"
                uiPalette: card.uiPalette
                primary: true
                text: "Áp dụng"
                enabled: card.ready && !card.busy && picker.currentIndex >= 0 && card.requestedValue !== card.runtimeDiagnostics
                onClicked: card.controller.applyDiagnostics(card.requestedValue)
            }
            SettingsButton {
                objectName: "reloadDiagnostics"
                uiPalette: card.uiPalette
                text: "Đọc lại"
                enabled: card.controller !== null && !card.busy
                onClicked: card.controller.reload()
            }
            BusyIndicator { running: card.busy; visible: running; implicitWidth: 24; implicitHeight: 24 }
        }
        Label {
            objectName: "diagnosticsFeedback"
            Layout.fillWidth: true
            text: feedback.text
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            text: "Áp dụng chỉ đổi DirectDiagnostics, không đổi chế độ gõ. Nên tắt sau khi thu thập đủ thông tin. Các tùy chọn nâng cao khác chưa tích hợp."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.smallSize
        }
    }
}
