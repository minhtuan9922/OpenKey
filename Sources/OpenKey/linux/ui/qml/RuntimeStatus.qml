import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    objectName: "runtimeStatus"
    required property var uiPalette
    property var overview: null
    property var controller: null
    readonly property string serviceState: overview !== null ? overview.serviceState : "disabled"
    readonly property string inputState: overview !== null ? overview.inputStatus : "unknown"
    readonly property string configState: overview !== null ? overview.configState : "not-read"
    readonly property string result: controller !== null ? controller.result : "offline"
    readonly property bool busy: controller !== null && controller.busy
    readonly property bool warning: ["error", "unavailable", "unconfirmed", "conflict", "invalid", "unsupported"].indexOf(result) >= 0 ||
        ["unavailable", "timeout", "protocol-error"].indexOf(serviceState) >= 0 ||
        ["invalid", "unreadable"].indexOf(configState) >= 0 || inputState === "unavailable"
    readonly property string summary: overview === null || controller === null ? "Chưa đủ nguồn trạng thái" :
        busy ? "Đang đọc / áp dụng cấu hình" : warning ? "Có mục cần kiểm tra" :
        controller.ready && ["loaded", "confirmed"].indexOf(result) >= 0 &&
        serviceState === "connected" && configState === "read" &&
        ["telex", "vni", "simple-telex1", "simple-telex2", "english", "inactive", "other"].indexOf(inputState) >= 0 ?
        "Có dữ liệu từ các nguồn" : "Chưa xác nhận đầy đủ"
    ConfigFeedback { id: runtimeFeedback; result: card.result }
    readonly property string serviceText: ({
        disabled: "Chưa truy vấn service trong phiên xem trước.",
        connecting: "Đang chờ phản hồi service…",
        connected: "Đã nhận mẫu trạng thái từ service.",
        unavailable: "Không lấy được phản hồi service. Không đồng nghĩa bộ gõ đã dừng.",
        timeout: "Service phản hồi quá hạn; mẫu trạng thái hiện không dùng được.",
        "protocol-error": "Phản hồi service không đúng giao thức; không dùng nội dung phản hồi."
    })[serviceState] || "Trạng thái service chưa xác định; không suy ra bộ gõ đang chạy hay đã dừng."
    readonly property string inputText: serviceState !== "connected" ? "Chưa có mẫu hợp lệ." : ({
        telex: "OpenKey Telex", vni: "OpenKey VNI", english: "English (US)",
        inactive: "Input method chưa active ở mẫu đã nhận", other: "Input method khác",
        unknown: "Service chưa xác định được input method",
        unavailable: "Service chưa lấy được trạng thái Fcitx5"
    })[inputState] || "Mẫu input method chưa xác định."
    readonly property string savedText: ({
        "not-read": "Chưa đọc file cấu hình.",
        read: "Đã đọc file cấu hình; không chứng minh runtime đã lưu thành công.",
        missing: "Chưa có file cấu hình; không kết luận runtime dùng giá trị mặc định.",
        invalid: "File cấu hình có dữ liệu không hợp lệ; không dùng giá trị trong file.",
        unreadable: "Không đọc được file cấu hình an toàn; không tự sửa hoặc ghi đè."
    })[configState] || "Trạng thái file cấu hình chưa xác định."
    readonly property string guidance: busy ? "Chờ thao tác cấu hình kết thúc; không tự gửi lại thay đổi." :
        result === "unconfirmed" ? "Thay đổi có thể đã áp dụng. Chỉ Đọc lại để đối chiếu, không tự Apply lại." :
        result === "conflict" ? "Cấu hình đã đổi ở nơi khác. Đọc lại trước khi chọn và Áp dụng lại có chủ đích." :
        result === "unsupported" ? "Kiểm tra phiên bản addon đang nạp; giao diện không tự nâng cấp hoặc restart Fcitx5." :
        warning ? "Nhấn Đọc lại trạng thái để đối chiếu. Kiểm tra service/Fcitx5 hoặc file cấu hình riêng; giao diện không tự sửa lỗi." :
        "Sau khi đổi cấu hình, kiểm thử gõ trong ứng dụng đích; dữ liệu ở đây không xác nhận Direct, gợi ý code hay lưu bền."
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
            text: "Kết nối & lỗi — " + card.summary
            objectName: "runtimeStatusSummary"
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            objectName: "runtimeStatusDetails"
            text: "Service: " + card.serviceText + "\nMẫu input method: " + card.inputText +
                "\nAPI cấu hình runtime: " + runtimeFeedback.text + "\nFile đã lưu: " + card.savedText
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.text
            Accessible.name: text
        }
        Label {
            Layout.fillWidth: true
            objectName: "runtimeStatusGuidance"
            text: card.guidance + "\n\nCác nguồn được lấy mẫu độc lập, không phải snapshot đồng thời hoặc kiểm tra sức khỏe engine. Mẫu input method thuộc context service quan sát, không phải ô đang focus. Đóng cửa sổ này không dừng bộ gõ."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
        }
        SettingsButton {
            objectName: "refreshRuntimeStatus"
            uiPalette: card.uiPalette
            text: "Đọc lại trạng thái"
            enabled: card.overview !== null && card.controller !== null && !card.busy
            onClicked: {
                card.overview.refresh()
                card.controller.reload()
            }
        }
    }
}
