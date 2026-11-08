import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: card
    objectName: "hotkeySettings"
    required property var uiPalette
    property var overview: null
    property var fileAccess: null
    FileDialog { id: openFile; title: "Mở phím tắt JSON"; onAccepted: { const data = card.fileAccess.loadHotkeys(selectedFile); if (data.ok) backup.text = data.text; importFeedback.text = data.ok ? "Đã mở JSON vào nháp." : "JSON phím tắt không hợp lệ." } }
    FileDialog { id: saveFile; title: "Lưu phím tắt JSON"; fileMode: FileDialog.SaveFile; defaultSuffix: "json"; onAccepted: importFeedback.text = card.fileAccess.saveHotkeys(selectedFile, backup.text) === "saved" ? "Đã lưu JSON." : "Không lưu được JSON." }

    readonly property string state: overview !== null ? overview.state : "offline"
    readonly property bool ready: state === "ready"
    readonly property var entries: [
        {key: "TriggerKeys", label: "Bật / tắt bộ gõ"},
        {key: "AltTriggerKeys", label: "Chuyển tạm thời"},
        {key: "ActivateKeys", label: "Bật bộ gõ"},
        {key: "DeactivateKeys", label: "Tắt bộ gõ"},
        {key: "EnumerateForwardKeys", label: "Bộ gõ tiếp theo"},
        {key: "EnumerateBackwardKeys", label: "Bộ gõ trước đó"},
        {key: "EnumerateGroupForwardKeys", label: "Nhóm bộ gõ tiếp theo"},
        {key: "EnumerateGroupBackwardKeys", label: "Nhóm bộ gõ trước đó"}
    ]
    function shortcutText(key) {
        if (!ready || overview === null) return "Chưa xác định"
        const keys = overview.shortcuts[key]
        return keys && keys.length > 0 ? keys.join(" / ") : "Chưa gán"
    }
    padding: uiPalette.cardPadding
    background: Rectangle {
        color: card.uiPalette.card
        radius: card.uiPalette.radius
        border.color: card.uiPalette.stroke
    }
    contentItem: ColumnLayout {
        spacing: 16
        Label {
            objectName: "uiNavigationShortcuts"
            Layout.fillWidth: true
            text: "Trong cửa sổ OpenKey: Ctrl+Tab / Ctrl+Shift+Tab chuyển trang; F1 mở Giới thiệu. Tab / Shift+Tab chuyển focus. Đây không phải phím EN/VI hoặc phím toàn hệ thống."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
        Label {
            Layout.fillWidth: true
            text: "Phím tắt Fcitx5"
            wrapMode: Text.Wrap
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            text: "Đọc cấu hình runtime của Fcitx5, dùng chung cho các bộ gõ; không phải phím tắt riêng của OpenKey. Các tên phím giữ nguyên theo Fcitx5."
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
        SettingsButton {
            objectName: "reloadHotkeys"
            uiPalette: card.uiPalette
            text: "Đọc lại"
            enabled: card.overview !== null && card.state !== "loading" && card.state !== "applying"
            onClicked: card.overview.reload()
        }
        Label {
            objectName: "hotkeyFeedback"
            Layout.fillWidth: true
            text: card.state === "ready" ? "Đã đọc cấu hình runtime; không xác nhận phím tắt đã được desktop hoặc ứng dụng chuyển tới Fcitx5." :
                card.state === "applying" ? "Đang áp dụng và đọc lại…" :
                card.state === "invalid" ? "Tên phím không hợp lệ hoặc vượt giới hạn; chưa gửi thay đổi." :
                card.state === "duplicate" ? "Phím bị trùng với hành động khác; chưa gửi thay đổi. Đọc lại trước khi sửa tiếp." :
                card.state === "conflict" ? "Cấu hình đã đổi ở nơi khác; Đọc lại và đối chiếu." :
                card.state === "unconfirmed" ? "Chưa xác nhận kết quả. Đọc lại trước khi thử tiếp; không tự gửi lại." :
                card.state === "loading" ? "Đang đọc phím tắt…" :
                card.state === "offline" ? "Đang offline; không đọc cấu hình phím tắt." :
                card.state === "error" ? "Không đọc được cấu hình/schema phím tắt tương thích. Không dùng giá trị mặc định thay thế." :
                "Không kết nối được Fcitx5 đang chạy. Giao diện không tự khởi động bộ gõ; bạn có thể Đọc lại."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Sửa một hành động mỗi lần. Mỗi dòng là một tổ hợp, tối đa 8 dòng; để trống để bỏ gán. Dùng tên Fcitx5 như Control+space, Shift+Alt+v, F6, Shift_L. Không tự bắt phím toàn hệ thống."
        }
        SettingsComboBox {
            id: editField
            objectName: "hotkeyEditField"
            Layout.fillWidth: true
            uiPalette: card.uiPalette
            model: card.entries.map(function(entry) { return entry.label })
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 100
            SettingsTextArea {
                uiPalette: card.uiPalette
                id: editKeys
                objectName: "hotkeyEditKeys"
                textFormat: TextEdit.PlainText
                placeholderText: "Ví dụ Control+space"
                Accessible.name: "Danh sách tổ hợp phím mới"
                color: card.uiPalette.text
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "loadHotkeyDraft"
                uiPalette: card.uiPalette; text: "Lấy phím runtime"
                enabled: card.ready
                onClicked: editKeys.text = card.overview.shortcuts[card.entries[editField.currentIndex].key].join("\n")
            }
            SettingsButton {
                objectName: "applyHotkeyDraft"
                uiPalette: card.uiPalette; text: "Áp dụng hành động đã chọn"; primary: true
                enabled: card.ready
                onClicked: card.overview.apply(card.entries[editField.currentIndex].key, editKeys.text)
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            SettingsTextArea {
                uiPalette: card.uiPalette
                id: backup
                objectName: "hotkeyBackupJson"
                textFormat: TextEdit.PlainText
                placeholderText: "JSON sao lưu phím tắt"
                Accessible.name: "JSON sao lưu phím tắt"
                selectByMouse: true
                color: card.uiPalette.text
            }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton {
                objectName: "exportHotkeys"
                uiPalette: card.uiPalette; text: "Xuất JSON runtime"
                enabled: card.ready
                onClicked: backup.text = card.overview.backupJson()
            }
            SettingsButton {
                objectName: "importHotkeyDraft"
                uiPalette: card.uiPalette; text: "Lấy hành động đã chọn từ JSON"
                enabled: card.ready && backup.length > 0
                onClicked: {
                    const result = card.overview.inspectBackup(backup.text)
                    if (result.ok) {
                        editKeys.text = result.shortcuts[card.entries[editField.currentIndex].key].join("\n")
                        importFeedback.text = "Đã nạp phím vào nháp; nhấn Áp dụng để khôi phục hành động đã chọn."
                    } else importFeedback.text = "JSON phím tắt không hợp lệ; giữ bản nháp."
                }
            }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton { uiPalette: card.uiPalette; text: "Mở JSON phím tắt…"; enabled: card.fileAccess !== null && card.state !== "applying"; onClicked: openFile.open() }
            SettingsButton { uiPalette: card.uiPalette; text: "Lưu JSON phím tắt…"; enabled: card.fileAccess !== null && card.fileAccess.validHotkeyBackup(backup.text) && card.state !== "applying"; onClicked: saveFile.open() }
        }
        Label { id: importFeedback; Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted }
        Repeater {
            model: card.entries
            delegate: ColumnLayout {
                required property var modelData
                Layout.fillWidth: true
                spacing: 4
                Label {
                    Layout.fillWidth: true
                    text: modelData.label
                    wrapMode: Text.Wrap
                    color: card.uiPalette.muted
                    font.pixelSize: card.uiPalette.bodySize
                }
                Label {
                    objectName: "hotkeyValue-" + modelData.key
                    Layout.fillWidth: true
                    text: card.shortcutText(modelData.key)
                    textFormat: Text.PlainText
                    wrapMode: Text.WrapAnywhere
                    color: card.uiPalette.text
                    font.pixelSize: card.uiPalette.bodySize
                    Accessible.name: modelData.label + ": " + text
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: "Muốn thay đổi: mở menu tray → Cấu hình bộ gõ Fcitx5… → Global Options. Chuyển tạm thời phụ thuộc trạng thái kích hoạt trước đó; chuyển nhóm không đồng nghĩa EN/VI hoặc Telex/VNI. GNOME/Wayland hay ứng dụng có thể giữ phím tắt trước Fcitx5. Chỉ nút Áp dụng ghi cấu hình Fcitx5. Kiểm tra/đọc lại trước ghi và xác nhận runtime sau ghi; không đăng ký phím toàn cục hoặc bắt phím bạn gõ."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
    }
}
