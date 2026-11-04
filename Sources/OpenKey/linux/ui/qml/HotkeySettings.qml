import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "hotkeySettings"
    required property var uiPalette
    property var overview: null
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
            text: "Phím tắt Fcitx5 — chỉ đọc"
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
            enabled: card.overview !== null && card.state !== "loading"
            onClicked: card.overview.reload()
        }
        Label {
            objectName: "hotkeyFeedback"
            Layout.fillWidth: true
            text: card.state === "ready" ? "Đã đọc cấu hình runtime; không xác nhận phím tắt đã được desktop hoặc ứng dụng chuyển tới Fcitx5." :
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
            text: "Muốn thay đổi: mở menu tray → Cấu hình bộ gõ Fcitx5… → Global Options. Chuyển tạm thời phụ thuộc trạng thái kích hoạt trước đó; chuyển nhóm không đồng nghĩa EN/VI hoặc Telex/VNI. GNOME/Wayland hay ứng dụng có thể giữ phím tắt trước Fcitx5. Trang này không ghi cấu hình, đăng ký phím toàn cục hoặc bắt phím bạn gõ."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
    }
}
