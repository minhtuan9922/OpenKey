import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: card
    objectName: "applicationMemoryBackup"
    required property var uiPalette
    property var controller: null
    property var fileAccess: null
    property var preview: ({ok: false})
    property string feedback: ""
    readonly property string mode: modePicker.currentIndex === 0 ? "merge" : "replace"
    readonly property bool connected: controller !== null && controller.ready && !controller.busy
    function methodLabel(method) {
        return ({"keyboard-us": "English (US)", "openkey-telex": "Telex", "openkey-vni": "VNI", "openkey-simple-telex1": "Simple Telex 1", "openkey-simple-telex2": "Simple Telex 2"})[method] || method
    }
    Connections {
        target: card.controller
        function onChanged() {
            if (card.preview.ok && card.controller !== null && !card.controller.backupPreviewReady) {
                card.preview = ({ok: false})
                card.feedback = card.controller.result === "applying" ? "" : "Bản xem trước đã hết hiệu lực. Nhấn Kiểm tra / xem trước lại."
            }
        }
    }
    function invalidate() {
        preview = ({ok: false})
        feedback = ""
        if (controller !== null) controller.cancelBackupPreview()
    }
    function loadFile(url) {
        if (fileAccess === null) return
        invalidate()
        const loaded = fileAccess.loadApplicationMemory(url)
        if (loaded.ok) {
            json.text = loaded.text
            feedback = "Đã mở file. Chọn cách nhập rồi xem trước; chưa đổi danh sách đã nhớ."
        } else feedback = "Không mở được file sao lưu danh sách ứng dụng hợp lệ."
    }
    function saveFile(url) {
        if (fileAccess === null) return
        feedback = fileAccess.saveApplicationMemory(url, json.text) === "saved" ? "Đã lưu file sao lưu." : "Không lưu được file sao lưu."
    }
    FileDialog {
        id: openDialog; objectName: "memoryBackupOpenDialog"
        title: "Mở bản sao bộ gõ theo ứng dụng"; fileMode: FileDialog.OpenFile
        nameFilters: ["JSON (*.json)"]
        onAccepted: card.loadFile(selectedFile)
    }
    FileDialog {
        id: saveDialog; objectName: "memoryBackupSaveDialog"
        title: "Lưu bản sao bộ gõ theo ứng dụng"; fileMode: FileDialog.SaveFile
        nameFilters: ["JSON (*.json)"]; defaultSuffix: "json"
        onAccepted: card.saveFile(selectedFile)
    }
    padding: 20
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { Layout.fillWidth: true; wrapMode: Text.Wrap; text: "Sao lưu bộ gõ theo ứng dụng"; color: card.uiPalette.text; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Sao lưu danh sách định danh ứng dụng và bộ gõ đã nhớ. Bản sao không chứa nội dung gõ. Mở file hoặc dán JSON, xem trước rồi xác nhận áp dụng."
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton {
                objectName: "exportMemoryBackup"; uiPalette: card.uiPalette; text: "Tạo JSON danh sách"
                enabled: card.connected
                onClicked: { card.invalidate(); json.text = card.controller.backupJson() }
            }
            SettingsButton {
                objectName: "openMemoryBackupFile"; uiPalette: card.uiPalette; text: "Mở file…"
                enabled: card.fileAccess !== null && (card.controller === null || !card.controller.busy)
                onClicked: openDialog.open()
            }
            SettingsButton {
                objectName: "saveMemoryBackupFile"; uiPalette: card.uiPalette; text: "Lưu thành file…"
                enabled: card.fileAccess !== null && card.fileAccess.validApplicationMemoryBackup(json.text)
                onClicked: saveDialog.open()
            }
        }
        ScrollView {
            Layout.fillWidth: true; Layout.preferredHeight: 150; contentWidth: availableWidth
            SettingsTextArea {
                id: json; objectName: "memoryBackupJson"; uiPalette: card.uiPalette
                textFormat: TextEdit.PlainText; selectByMouse: true; wrapMode: TextEdit.Wrap
                Accessible.name: "JSON sao lưu bộ gõ theo ứng dụng"
                placeholderText: "JSON danh sách ứng dụng đã nhớ"
                onTextChanged: card.invalidate()
            }
        }
        SettingsComboBox {
            id: modePicker; objectName: "memoryBackupMode"; uiPalette: card.uiPalette; Layout.fillWidth: true
            model: ["Gộp — giữ mục khác, file nhập ưu tiên khi trùng", "Thay thế — dùng toàn bộ danh sách trong file"]
            Accessible.name: "Cách nhập danh sách ứng dụng"
            onCurrentIndexChanged: card.invalidate()
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.mode === "replace" ? "Thay thế sẽ xóa các mục không có trong file. File có danh sách rỗng sẽ xóa toàn bộ bảng nhớ." :
                "Gộp giữ mục hiện có; nếu cùng định danh, bộ gõ trong file sẽ được dùng. Tổng tối đa 128 ứng dụng."
        }
        SettingsButton {
            objectName: "inspectMemoryBackup"; uiPalette: card.uiPalette; text: "Kiểm tra / xem trước"
            enabled: card.connected && json.length > 0
            onClicked: {
                card.preview = card.controller.inspectBackup(json.text, card.mode)
                card.feedback = card.preview.ok ? "Đã xem trước; chưa áp dụng. Kiểm tra các thay đổi bên dưới rồi xác nhận." :
                    card.preview.status === "capacity" ? "Kết quả vượt quá 128 ứng dụng. Giảm danh sách hoặc chọn Thay thế." : "JSON không hợp lệ hoặc chưa kết nối addon hỗ trợ."
            }
        }
        Label {
            objectName: "memoryBackupSummary"; visible: card.preview.ok === true
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.text
            text: card.preview.ok ? "Thêm: " + card.preview.added + " · Sửa: " + card.preview.updated + " · Xóa: " + card.preview.removed + " · Sau khi nhập: " + card.preview.total : ""
        }
        Repeater {
            model: card.preview.ok ? card.preview.changes : []
            delegate: Label {
                required property var modelData
                Layout.fillWidth: true; wrapMode: Text.Wrap; textFormat: Text.PlainText; color: card.uiPalette.text
                text: modelData.application + ": " + (card.methodLabel(modelData.before) || "Chưa có") + " → " + (card.methodLabel(modelData.after) || "Xóa")
            }
        }
        SettingsButton {
            objectName: "restoreMemoryBackup"; uiPalette: card.uiPalette; text: "Xác nhận áp dụng danh sách"; primary: true
            enabled: card.connected && card.preview.ok === true && card.controller.backupPreviewReady
            onClicked: { card.controller.restoreBackup(json.text, card.mode); card.preview = ({ok: false}); card.feedback = "" }
        }
        ConfigFeedback { id: resultFeedback; result: card.controller !== null ? card.controller.result : "offline" }
        Label {
            objectName: "memoryBackupFeedback"; Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.feedback || (card.controller !== null && card.controller.result === "confirmed" ? "Đã xác nhận danh sách đã nhớ. Có hiệu lực khi quay lại ứng dụng." : resultFeedback.text)
        }
    }
}
