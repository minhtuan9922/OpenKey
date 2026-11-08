import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: card
    objectName: "inputSettingsBackup"
    required property var uiPalette
    property var controller: null
    property var fileAccess: null
    property var preview: ({ok: false})
    property string feedback: ""
    readonly property bool available: controller !== null && controller.ready && !controller.busy
    readonly property bool fileActionsAvailable: fileAccess !== null && (controller === null || !controller.busy)
    function loadFile(url) {
        if (!fileActionsAvailable) return
        card.preview = ({ok: false})
        const loaded = fileAccess.load(url)
        if (loaded.ok) {
            backup.text = loaded.text
            card.feedback = "Đã mở bản sao lưu. Nhấn Kiểm tra / xem trước trước khi khôi phục; chưa thay đổi bộ gõ."
        } else card.feedback = "Không mở được file JSON cấu hình bộ gõ hợp lệ (UTF-8, tối đa 4096 đơn vị UTF-16). Giữ nguyên nội dung đang sửa."
    }
    function saveFile(url) {
        if (!fileActionsAvailable) return
        const status = fileAccess.save(url, backup.text)
        card.feedback = status === "saved" ? "Đã lưu file JSON được chọn. Chưa thay đổi cấu hình bộ gõ." :
            "Không lưu được bản sao. Kiểm tra JSON, đường dẫn hoặc quyền ghi; nội dung đang sửa được giữ nguyên."
    }
    FileDialog {
        id: openDialog
        objectName: "inputBackupOpenDialog"
        title: "Mở bản sao lưu cấu hình bộ gõ"
        fileMode: FileDialog.OpenFile
        nameFilters: ["Bản sao lưu JSON (*.json)"]
        onAccepted: card.loadFile(selectedFile)
    }
    FileDialog {
        id: saveDialog
        objectName: "inputBackupSaveDialog"
        title: "Lưu bản sao cấu hình bộ gõ"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Bản sao lưu JSON (*.json)"]
        defaultSuffix: "json"
        onAccepted: card.saveFile(selectedFile)
    }
    function valueLabel(key, value) {
        if (key === "InputStyle") return ({Preedit: "Preedit", Direct: "Direct", CommitOnly: "CommitOnly", NativeDirect: "NativeDirect"})[value] || "Chưa xác định"
        return value === "True" ? "Bật" : "Tắt"
    }
    function previewText() {
        if (!preview.ok) return ""
        const labels = {InputStyle: "Chế độ gõ", DirectDiagnostics: "Chẩn đoán", CheckSpelling: "Kiểm tra chính tả",
            UseModernOrthography: "Đặt dấu kiểu mới", QuickTelex: "Gõ nhanh Telex", FreeMark: "Đặt dấu tự do", AllowConsonantZFWJ: "Phụ âm Z F W J",
            QuickStartConsonant: "Phụ âm đầu", QuickEndConsonant: "Phụ âm cuối", RestoreWrongSpelling: "Khôi phục từ sai", RememberInputMethodPerApplication: "Nhớ bộ gõ theo ứng dụng", UseGnomeApplicationIdentity: "Nhận diện ứng dụng GNOME"}
        const lines = []
        for (const key of Object.keys(labels)) {
            if (preview.settings[key] !== undefined)
                lines.push(labels[key] + ": " + valueLabel(key, preview.current[key]) + " → " + valueLabel(key, preview.settings[key]))
        }
        return lines.join("\n")
    }
    Connections {
        target: card.controller
        function onChanged() { card.preview = ({ok: false}) }
    }
    onControllerChanged: preview = ({ok: false})
    ConfigFeedback { id: result; result: card.controller !== null ? card.controller.result : "offline" }
    padding: uiPalette.cardPadding
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: "Sao lưu / khôi phục cấu hình bộ gõ"
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label {
            text: "Tạo JSON từ cấu hình đang chạy rồi lưu thành file, hoặc mở bản sao đã lưu. Bạn cũng có thể sao chép/dán JSON thủ công. Luôn xem trước trước khi khôi phục. Bản sao không chứa phím tắt Fcitx5, gõ tắt, lựa chọn Telex/VNI hoặc theme."
            color: card.uiPalette.muted
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        SettingsButton {
            objectName: "exportInputBackup"
            uiPalette: card.uiPalette
            text: "Tạo JSON bộ gõ"
            enabled: card.available
            onClicked: {
                backup.text = card.controller.backupJson()
                card.preview = ({ok: false})
                card.feedback = backup.text.length > 0 ? "Đã tạo JSON từ runtime; chưa ghi file hoặc clipboard." : "Không tạo được bản sao; hãy Đọc lại cấu hình."
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "openInputBackupFile"
                uiPalette: card.uiPalette
                text: "Mở file JSON…"
                enabled: card.fileActionsAvailable
                onClicked: openDialog.open()
            }
            SettingsButton {
                objectName: "saveInputBackupFile"
                uiPalette: card.uiPalette
                text: "Lưu thành file…"
                enabled: card.fileActionsAvailable && card.fileAccess.validBackup(backup.text)
                onClicked: saveDialog.open()
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            contentWidth: availableWidth
            clip: true
            TextArea {
                id: backup
                objectName: "inputBackupText"
                Accessible.name: "JSON sao lưu cấu hình bộ gõ"
                placeholderText: "Dán JSON cấu hình bộ gõ OpenKey…"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                enabled: card.controller === null || !card.controller.busy
                color: card.uiPalette.text
                placeholderTextColor: card.uiPalette.muted
                selectionColor: card.uiPalette.accent
                selectedTextColor: card.uiPalette.accentText
                padding: 12
                background: Rectangle {
                    color: card.uiPalette.page
                    border.color: backup.activeFocus ? card.uiPalette.accent : card.uiPalette.stroke
                    radius: 5
                }
                onTextChanged: { card.preview = ({ok: false}); card.feedback = "" }
            }
        }
        Label {
            text: backup.length + " / 4096 đơn vị UTF-16. Sửa JSON hoặc đọc lại runtime sẽ hủy bản xem trước."
            color: card.uiPalette.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        SettingsButton {
            objectName: "previewInputBackup"
            text: "Kiểm tra / xem trước cấu hình"
            uiPalette: card.uiPalette
            enabled: card.available && backup.length > 0 && backup.length <= 4096
            onClicked: {
                card.preview = card.controller.inspectBackup(backup.text)
                card.feedback = card.preview.ok ? "JSON hợp lệ. Kiểm tra các giá trị dưới đây; kết thúc từ đang gõ rồi nhấn Khôi phục để xác nhận." :
                    card.preview.status === "unsupported" ? "Addon đang chạy chưa hỗ trợ đầy đủ các thiết lập trong bản sao. Chưa gửi thay đổi." :
                    "JSON không hợp lệ hoặc chưa đọc được runtime. Chưa gửi thay đổi."
            }
        }
        Label {
            objectName: "inputBackupPreview"
            visible: card.preview.ok === true
            text: card.previewText()
            textFormat: Text.PlainText
            color: card.uiPalette.text
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        SettingsButton {
            objectName: "restoreInputBackup"
            text: "Khôi phục cấu hình bộ gõ"
            uiPalette: card.uiPalette
            primary: true
            enabled: card.available && card.preview.ok === true
            onClicked: {
                card.feedback = ""
                card.preview = ({ok: false})
                card.controller.restoreBackup(backup.text)
            }
        }
        Label {
            objectName: "inputBackupFeedback"
            text: card.feedback.length > 0 ? card.feedback : result.text
            textFormat: Text.PlainText
            color: card.uiPalette.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            Accessible.name: text
        }
    }
}
