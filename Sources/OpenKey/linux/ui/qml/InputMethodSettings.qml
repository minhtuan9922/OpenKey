import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
Pane {
    id: card
    objectName: "inputMethodSettings"
    required property var uiPalette
    property var controller: null
    property var fileAccess: null
    property string backupStatus: ""
    FileDialog { id: openBackup; title: "Mở lựa chọn bộ gõ JSON"; onAccepted: { const data = card.fileAccess.loadMethod(selectedFile); if (data.ok) backup.text = data.text; card.backupStatus = data.ok ? "Đã mở vào nháp." : "File lựa chọn bộ gõ không hợp lệ." } }
    FileDialog { id: saveBackup; title: "Lưu lựa chọn bộ gõ JSON"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; onAccepted: card.backupStatus = card.fileAccess.saveMethod(selectedFile, backup.text) === "saved" ? "Đã lưu JSON." : "Không lưu được JSON." }

    readonly property var names: ({"openkey-telex": "OpenKey Telex", "openkey-vni": "OpenKey VNI",
        "openkey-simple-telex1": "OpenKey Simple Telex 1", "openkey-simple-telex2": "OpenKey Simple Telex 2", "keyboard-us": "Tiếng Anh (US)"})
    readonly property var configured: controller !== null ? controller.methods : []
    readonly property bool busy: controller !== null && controller.busy
    readonly property bool ready: controller !== null && controller.ready
    function syncPicker() {
        if (controller !== null && controller.ready) picker.currentIndex = controller.methods.indexOf(controller.current)
        else if (!busy) picker.currentIndex = -1
    }
    onConfiguredChanged: Qt.callLater(syncPicker)
    onControllerChanged: Qt.callLater(syncPicker)
    Connections { target: card.controller; function onChanged() { card.syncPicker() } }
    padding: uiPalette.cardPadding
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Chọn bộ gõ"; color: card.uiPalette.text; font.pixelSize: card.uiPalette.headingSize; font.bold: true }
        Label {
            objectName: "currentInputMethod"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.text
            text: !card.ready ? "Bộ gõ hiện tại: chưa xác định" :
                "Bộ gõ hiện tại: " + (card.names[card.controller.current] || "Bộ gõ khác") +
                (card.controller.active || card.controller.current === "keyboard-us" ? "" : " — chưa kích hoạt")
        }
        SettingsComboBox {
            id: picker
            objectName: "inputMethodPicker"
            uiPalette: card.uiPalette
            Layout.fillWidth: true
            model: card.configured.map(function(key) { return card.names[key] })
            currentIndex: -1
            displayText: currentIndex >= 0 ? currentText : "Chọn bộ gõ đã thêm vào Fcitx5"
            enabled: card.ready && !card.busy && count > 0
            Accessible.name: "Chọn Telex, VNI, Simple Telex hoặc tiếng Anh"
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton {
                objectName: "selectInputMethod"
                uiPalette: card.uiPalette; text: "Chuyển bộ gõ"; primary: true
                enabled: card.ready && !card.busy && picker.currentIndex >= 0 &&
                    (card.configured[picker.currentIndex] !== card.controller.current || !card.controller.active)
                onClicked: card.controller.select(card.configured[picker.currentIndex])
            }
            SettingsButton {
                objectName: "reloadInputMethods"
                uiPalette: card.uiPalette; text: "Đọc lại"
                enabled: card.controller !== null && !card.busy
                onClicked: card.controller.reload()
            }
        }
        Label {
            objectName: "inputMethodFeedback"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.controller === null ? "Đang xem trước; không chuyển bộ gõ." : ({
                offline: "Chưa đọc danh sách bộ gõ.", loading: "Đang đọc từ Fcitx5…", applying: "Đang chuyển và đọc lại để xác nhận…",
                loaded: "Đã đọc nhóm Fcitx5 hiện tại.", confirmed: "Đã xác nhận bộ gõ được chọn từ Fcitx5.",
                unavailable: "Không kết nối được Fcitx5 đang chạy. Không tự khởi động daemon.",
                "not-configured": "Kiểu gõ chưa có trong nhóm hiện tại. Thêm trong cấu hình Fcitx5 rồi Đọc lại.",
                conflict: "Nhóm hoặc bộ gõ đã đổi ở nơi khác. Đọc lại trước khi chọn tiếp.",
                unconfirmed: "Chưa xác nhận chuyển bộ gõ. Đọc lại trước khi thử tiếp; không tự gửi lại.",
                invalid: "Lựa chọn không hợp lệ.", error: "Không đọc được phản hồi tương thích từ Fcitx5."
            })[card.controller.result] || "Chưa xác định"
        }
        ScrollView {
            Layout.fillWidth: true; Layout.preferredHeight: 100
            SettingsTextArea { id: backup; objectName: "inputMethodBackupJson"; uiPalette: card.uiPalette; placeholderText: "JSON sao lưu lựa chọn bộ gõ" }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton { objectName: "exportInputMethod"; uiPalette: card.uiPalette; text: "Xuất lựa chọn JSON"; enabled: card.ready && !card.busy; onClicked: backup.text = card.controller.backupJson() }
            SettingsButton {
                objectName: "inspectInputMethodBackup"; uiPalette: card.uiPalette; text: "Lấy lựa chọn từ JSON"; enabled: card.ready && !card.busy && backup.length > 0
                onClicked: {
                    const result = card.controller.inspectBackup(backup.text)
                    if (result.ok && card.configured.indexOf(result.method) >= 0) {
                        picker.currentIndex = card.configured.indexOf(result.method)
                        card.backupStatus = "Đã chọn vào nháp; nhấn Chuyển bộ gõ để khôi phục."
                    } else card.backupStatus = result.ok ? "Bộ gõ trong JSON chưa có trong nhóm Fcitx5 hiện tại." : "JSON lựa chọn bộ gõ không hợp lệ."
                }
            }
            SettingsButton { uiPalette: card.uiPalette; text: "Mở JSON…"; enabled: card.fileAccess !== null && !card.busy; onClicked: openBackup.open() }
            SettingsButton { uiPalette: card.uiPalette; text: "Lưu JSON…"; enabled: card.fileAccess !== null && !card.busy && card.fileAccess.validMethodBackup(backup.text); onClicked: saveBackup.open() }
        }
        Label { Layout.fillWidth: true; text: card.backupStatus; wrapMode: Text.Wrap; color: card.uiPalette.muted }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Danh sách chỉ gồm các kiểu đã thêm vào nhóm Fcitx5 hiện tại. Nếu thiếu Simple Telex, mở Fcitx5 Configuration → Input Method → Add, bỏ Only Show Current Language rồi thêm OpenKey Simple Telex 1/2.\n\nLựa chọn được gửi khi nhấn Chuyển bộ gõ. Trạng thái thuộc context gần nhất do Fcitx5 quản lý; ứng dụng có cấu hình giữ bộ gõ riêng có thể khôi phục lựa chọn khác khi nhận focus."
        }
    }
}
