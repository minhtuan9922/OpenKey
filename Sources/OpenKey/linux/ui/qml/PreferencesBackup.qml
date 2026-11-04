import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "preferencesBackup"
    required property var uiPalette
    property var preferences: null
    property string currentTheme: "system"
    property bool currentSidebarExpanded: true
    property var preview: ({ok: false})
    property string feedback: ""
    signal restored(string theme, bool sidebarExpanded)
    padding: uiPalette.cardPadding
    background: Rectangle {
        color: card.uiPalette.card
        radius: card.uiPalette.radius
        border.color: card.uiPalette.stroke
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: "Sao lưu / khôi phục giao diện"
            wrapMode: Text.Wrap
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            text: "Chỉ gồm theme và trạng thái mở/thu gọn menu. Không chứa cấu hình Fcitx5, phím tắt, gõ tắt hoặc văn bản Chuyển mã. Tạo JSON rồi chọn và Ctrl+C để lưu thủ công; dán JSON vào ô dưới để xem trước. Không tự đọc/ghi file sao lưu hoặc clipboard."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
        }
        SettingsButton {
            objectName: "exportUiBackup"
            uiPalette: card.uiPalette
            text: "Tạo JSON giao diện"
            enabled: card.preferences !== null
            onClicked: {
                card.preview = ({ok: false})
                backup.text = card.preferences.backupJson(card.currentTheme, card.currentSidebarExpanded)
                card.feedback = backup.text.length > 0 ? "Đã tạo JSON trong ô nhập; chưa ghi file hoặc clipboard." :
                    "Không tạo được JSON; lựa chọn giao diện hiện tại không hợp lệ."
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            contentWidth: availableWidth
            clip: true
            background: Rectangle {
                color: card.uiPalette.card
                radius: 5
                border.width: backup.activeFocus ? 2 : 1
                border.color: backup.activeFocus ? card.uiPalette.accent : card.uiPalette.stroke
            }
            TextArea {
                id: backup
                objectName: "uiBackupText"
                placeholderText: "JSON sao lưu giao diện OpenKey"
                Accessible.name: "JSON sao lưu giao diện"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: card.uiPalette.text
                placeholderTextColor: card.uiPalette.muted
                selectionColor: card.uiPalette.accent
                selectedTextColor: card.uiPalette.accentText
                font.pixelSize: card.uiPalette.bodySize
                padding: 12
                background: null
                onTextChanged: { card.preview = ({ok: false}); card.feedback = "" }
            }
        }
        Label {
            Layout.fillWidth: true
            text: backup.text.length + " / 4096 đơn vị UTF-16. Đổi JSON sẽ hủy xác nhận xem trước."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.smallSize
        }
        SettingsButton {
            objectName: "previewUiBackup"
            uiPalette: card.uiPalette
            text: "Kiểm tra / xem trước"
            enabled: card.preferences !== null && backup.text.length > 0
            onClicked: {
                card.preview = card.preferences.inspectBackup(backup.text)
                card.feedback = card.preview.ok ? "JSON hợp lệ; chưa thay đổi giao diện. Nhấn Khôi phục để xác nhận." :
                    card.preview.status === "too-large" ? "JSON vượt giới hạn 4096 đơn vị UTF-16; không thay đổi cấu hình." :
                    "JSON không hợp lệ hoặc không đúng phiên bản sao lưu giao diện OpenKey; không thay đổi cấu hình."
            }
        }
        Label {
            objectName: "uiBackupPreview"
            Layout.fillWidth: true
            visible: card.preview.ok === true
            text: card.preview.ok ? "Sẽ khôi phục: theme " + card.preview.theme + "; menu " +
                (card.preview.sidebarExpanded ? "mở rộng" : "thu gọn") + "." : ""
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.text
        }
        SettingsButton {
            objectName: "restoreUiBackup"
            uiPalette: card.uiPalette
            primary: true
            text: "Khôi phục giao diện"
            enabled: card.preferences !== null && card.preview.ok === true
            onClicked: {
                if (card.preferences.restoreBackup(backup.text)) {
                    card.restored(card.preferences.themeMode, card.preferences.sidebarExpanded)
                    card.feedback = "Đã lưu và khôi phục giao diện. Không thay đổi bộ gõ hoặc khởi động lại dịch vụ."
                } else {
                    card.feedback = "Không khôi phục được; giữ nguyên giao diện. Kiểm tra JSON, file cấu hình hoặc quyền ghi."
                }
                card.preview = ({ok: false})
            }
        }
        Label {
            objectName: "uiBackupFeedback"
            Layout.fillWidth: true
            text: card.preferences === null ? "Xem trước offline: không xuất/khôi phục cấu hình đã lưu." : card.feedback
            textFormat: Text.PlainText
            Accessible.name: text
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
        }
    }
}
