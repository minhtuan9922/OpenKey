import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: page
    objectName: "compatibilitySettings"
    required property var uiPalette
    property var controller: null
    property var environment: ({})
    property bool showDetails: true
    readonly property string sessionLabel: ({
        wayland: "Wayland (XDG_SESSION_TYPE)",
        x11: "X11 (XDG_SESSION_TYPE)",
        tty: "TTY (XDG_SESSION_TYPE)"
    })[environment.sessionType] || "Chưa xác định"
    readonly property string platformLabel: ({
        wayland: "wayland — cửa sổ Qt dùng Wayland",
        "wayland-egl": "wayland-egl — cửa sổ Qt dùng Wayland",
        xcb: "xcb — cửa sổ Qt dùng X11, có thể qua XWayland",
        offscreen: "offscreen — render không có cửa sổ desktop",
        minimal: "minimal — backend Qt tối giản",
        vnc: "vnc — backend Qt VNC",
        eglfs: "eglfs — backend Qt EGLFS",
        linuxfb: "linuxfb — backend Qt framebuffer"
    })[environment.platformName] || "Chưa xác định"
    spacing: uiPalette.sectionSpacing
    InfoCard {
        objectName: "compatibilityEnvironmentCard"
        visible: page.showDetails
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "Môi trường của cửa sổ OpenKey"
        badge: "Chỉ đọc"
        description: "Phiên đăng nhập: " + page.sessionLabel +
            "\nBackend cửa sổ này: " + page.platformLabel +
            "\nAPI cấu hình addon: " + (page.controller !== null && page.controller.ready ?
                "Đã đọc schema/runtime" : "Chưa xác nhận") +
            "\n\nLoại phiên lấy từ biến môi trường, không phải kiểm chứng compositor. Backend Qt chỉ thuộc cửa sổ OpenKey, không xác định backend của VS Code, Chrome hoặc ô đang gõ."
    }
    StyleSettings {
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        controller: page.controller
    }
    InfoCard {
        objectName: "compatibilityDirectCard"
        visible: page.showDetails
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "Direct không đồng nghĩa mọi ứng dụng đều không gạch chân"
        badge: "Giới hạn frontend"
        description: "Preedit giữ composition để tương thích. Direct commit ký tự và thay dấu qua surrounding-text khi ứng dụng cung cấp dữ liệu hợp lệ; thiếu capability có thể chuyển về Preedit. Mất đồng bộ giữa từ có thể làm phím Telex/VNI giữ nguyên.\n\nRuntime Direct ở trên là cấu hình chung, không xác nhận Direct đang hoạt động trong ứng dụng khác. Trang này không đọc cửa sổ đang focus, nội dung đang gõ hoặc diagnostics log. Khi cần điều tra, bật chẩn đoán có chủ đích ở trang Nâng cao; không có tự bật chẩn đoán."
    }
    InfoCard {
        objectName: "compatibilityVscodeCard"
        visible: page.showDetails
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "VS Code — đối chứng trên profile riêng"
        badge: "Không tự sửa settings"
        description: "Trên máy đã kiểm thử, profile accessible với editor.editContext=false và editor.accessibilitySupport=on gõ được Direct và giữ gợi ý. Đây là kết quả ở profile thử, không bảo đảm mọi phiên bản hoặc backend. Accessibility support có thể thay đổi hành vi editor.\n\nXem hướng dẫn Ubuntu install / VS Code input investigation trong tài liệu dự án. Dùng script scripts/test-vscode-input.sh để tạo profile đối chứng thay vì sửa profile chính; giao diện không chạy script, mở editor hoặc thay cấu hình VS Code."
    }
    InfoCard {
        objectName: "compatibilityScopeCard"
        visible: page.showDetails
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "Phạm vi áp dụng và kiểm thử"
        badge: "Tích hợp một phần"
        description: "Chế độ gõ áp dụng chung cho Telex/VNI và mọi ứng dụng, không phải cấu hình riêng từng app. Chỉ nhấn Áp dụng khi đã gõ xong từ; không tự chuyển EN/VI hoặc restart Fcitx5.\n\nSau khi đổi, thử đặt dấu, Backspace, xóa Space để sửa tiếp, gợi ý code, Tab/Enter nhận completion, undo/redo và đổi focus. Trong code cần nguyên trạng, chuyển English theo phím/menu Fcitx5. Danh sách ứng dụng, tự nhận diện backend ứng dụng và quy tắc theo app chưa tích hợp."
    }
}
