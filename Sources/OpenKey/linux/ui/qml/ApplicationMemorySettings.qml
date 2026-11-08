import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "applicationMemorySettings"
    required property var uiPalette
    property var controller: null
    property string selectedApplication: ""
    property string expectedMethod: ""
    readonly property var methods: ["keyboard-us", "openkey-telex", "openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2"]
    readonly property var methodLabels: ["English (US)", "Telex", "VNI", "Simple Telex 1", "Simple Telex 2"]
    readonly property bool connected: controller !== null && controller.ready && !controller.busy
    function label(method) { return methodLabels[methods.indexOf(method)] || method }
    function reload() { if (controller !== null && !controller.busy) controller.reload() }
    onVisibleChanged: { if (visible) reload() }
    Component.onCompleted: { if (visible) reload() }
    Timer {
        interval: 2000; repeat: true
        running: card.visible && card.controller !== null && !card.controller.backupPreviewReady && (card.controller.result === "loaded" || card.controller.result === "offline")
        onTriggered: card.controller.refresh()
    }
    Connections {
        target: card.controller
        function onChanged() {
            if (card.controller !== null && card.controller.result === "confirmed") {
                card.selectedApplication = ""
                card.expectedMethod = ""
            }
        }
    }
    padding: 20
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Bộ gõ đã nhớ theo ứng dụng"; color: card.uiPalette.text; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Xem, sửa hoặc xóa từng mục đã nhớ. Thay đổi có hiệu lực khi quay lại ứng dụng; không đổi bộ gõ đang dùng. Nếu vẫn bật ghi nhớ, mục đã xóa có thể được tự nhớ lại ở lần dùng tiếp theo."
        }
        Label {
            objectName: "memoryIdentityStatus"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.text
            text: "Kết nối GNOME (lần quan sát gần nhất): " + (card.controller === null ? "Chưa kết nối" :
                ({disabled: "Đang tắt chức năng", "not-observed": "Chưa kiểm tra — chuyển sang ứng dụng để kiểm tra", waiting: "Chờ xác nhận focus", connected: "Đã nhận phản hồi từ extension", unavailable: "Chưa kết nối được extension"}[card.controller.identityStatus] || "Chưa xác định"))
        }
        Label {
            objectName: "memoryCurrentApplication"
            Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; color: card.uiPalette.text
            text: "Ứng dụng đang nhận diện: " + (card.controller !== null && card.controller.currentApplication.length > 0 ? card.controller.currentApplication : "Chưa có định danh được xác nhận")
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Khi cửa sổ cài đặt có focus, định danh có thể là chính OpenKey Settings. Ô nhạy cảm hoặc focus chưa xác nhận không hiển thị định danh. Trạng thái GNOME là lần quan sát gần nhất, không phải kiểm tra kết nối liên tục."
        }
        SettingsButton { objectName: "reloadApplicationMemory"; uiPalette: card.uiPalette; text: "Đọc lại danh sách và trạng thái"; enabled: card.controller !== null && !card.controller.busy; onClicked: card.reload() }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.controller === null || !card.controller.ready ? "Cần addon hỗ trợ quản lý bộ gõ theo ứng dụng (alpha.53 trở lên)." :
                card.controller.entries.length === 0 ? "Chưa có ứng dụng nào được nhớ. Bật ghi nhớ ở trên, chuyển sang ứng dụng và chọn bộ gõ." : card.controller.entries.length + " / 128 ứng dụng đã nhớ"
        }
        Repeater {
            model: card.controller !== null && card.controller.ready ? card.controller.entries : []
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true; spacing: 6
                Label { Layout.fillWidth: true; text: modelData.application + " → " + card.label(modelData.method); textFormat: Text.PlainText; wrapMode: Text.Wrap; color: card.uiPalette.text }
                Flow {
                    Layout.fillWidth: true; spacing: 8
                    SettingsButton {
                        objectName: "editRememberedApplication"
                        uiPalette: card.uiPalette; text: "Sửa"; enabled: card.connected
                        onClicked: {
                            card.selectedApplication = modelData.application
                            card.expectedMethod = modelData.method
                            methodPicker.currentIndex = card.methods.indexOf(modelData.method)
                        }
                    }
                    SettingsButton {
                        objectName: "deleteRememberedApplication"
                        uiPalette: card.uiPalette; text: "Xóa"; enabled: card.connected
                        onClicked: card.controller.remove(modelData.application, modelData.method)
                    }
                }
            }
        }
        Label {
            visible: card.selectedApplication.length > 0
            Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; color: card.uiPalette.text
            text: "Đang sửa: " + card.selectedApplication
        }
        SettingsComboBox {
            id: methodPicker; objectName: "rememberedMethodPicker"
            visible: card.selectedApplication.length > 0
            Layout.fillWidth: true; uiPalette: card.uiPalette; model: card.methodLabels
            Accessible.name: "Bộ gõ nhớ cho ứng dụng đang sửa"
        }
        SettingsButton {
            objectName: "saveRememberedApplication"
            visible: card.selectedApplication.length > 0
            uiPalette: card.uiPalette; text: "Lưu bộ gõ cho ứng dụng"; primary: true
            enabled: card.connected && methodPicker.currentIndex >= 0 && card.methods[methodPicker.currentIndex] !== card.expectedMethod
            onClicked: card.controller.update(card.selectedApplication, card.expectedMethod, card.methods[methodPicker.currentIndex])
        }
        ConfigFeedback { id: feedback; result: card.controller !== null ? card.controller.result : "offline" }
        Label {
            objectName: "applicationMemoryFeedback"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.controller !== null && card.controller.result === "confirmed" ? "Đã lưu danh sách. Chọn Đọc lại để tiếp tục cập nhật trạng thái." :
                card.controller !== null && card.controller.result === "conflict" ? "Danh sách đã thay đổi. Đọc lại và chọn Sửa lại trước khi lưu." : feedback.text
        }
    }
}
