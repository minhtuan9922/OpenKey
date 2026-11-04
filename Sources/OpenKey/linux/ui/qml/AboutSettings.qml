import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ColumnLayout {
    id: page
    required property var uiPalette
    property var info: null
    readonly property bool licenseOpen: info !== null && info.licenseState === "ready"
    spacing: uiPalette.sectionSpacing
    InfoCard {
        objectName: "aboutVersionCard"
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "OpenKey cho Ubuntu"
        badge: "v0.4 UI preview"
        description: "Phiên bản nguồn UI: " + (page.info !== null ? page.info.sourceVersion : "Chưa xác định") +
            "\nQt khi biên dịch: " + (page.info !== null ? page.info.qtBuildVersion : "Chưa xác định") +
            "\nQt đang chạy: " + (page.info !== null ? page.info.qtRuntimeVersion : "Chưa xác định") +
            "\n\nĐây là thông tin binary giao diện, không phải phiên bản addon Fcitx5 đang nạp. Không suy ra alpha.11 đang chạy chỉ vì UI có Gõ nhanh Telex. Bộ gõ và tray hoạt động độc lập; đóng UI không dừng chúng."
    }
    InfoCard {
        objectName: "aboutCreditsCard"
        Layout.fillWidth: true
        uiPalette: page.uiPalette
        heading: "Tác giả và phạm vi"
        badge: "Mã nguồn mở"
        description: "Engine OpenKey gốc: Mai Vũ Tuyên. Bản fork bổ sung tích hợp Linux/Fcitx5 và giao diện Qt Quick theo bố cục Windows. Bản Ubuntu vẫn đang phát triển, chưa tương đương toàn bộ chức năng Windows.\n\nKhông có kiểm tra/tải cập nhật tự động trên trang này. Không gửi cấu hình, diagnostics hoặc nội dung đang gõ lên mạng."
    }
    Pane {
        Layout.fillWidth: true
        padding: page.uiPalette.cardPadding
        background: Rectangle { color: page.uiPalette.card; radius: page.uiPalette.radius; border.color: page.uiPalette.stroke }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                text: "Nguồn dự án"
                color: page.uiPalette.text
                font.pixelSize: page.uiPalette.headingSize
                font.weight: Font.DemiBold
            }
            TextArea {
                objectName: "aboutProjectSources"
                Layout.fillWidth: true
                text: "Bản fork: https://github.com/minhtuan9922/OpenKey\nOpenKey gốc: https://github.com/tuyenvm/OpenKey"
                textFormat: TextEdit.PlainText
                readOnly: true
                selectByMouse: true
                wrapMode: TextEdit.WrapAnywhere
                color: page.uiPalette.text
                selectionColor: page.uiPalette.accent
                selectedTextColor: page.uiPalette.accentText
                background: null
                Accessible.name: "Địa chỉ mã nguồn dự án, có thể chọn và sao chép"
            }
            Label {
                Layout.fillWidth: true
                text: "Địa chỉ chỉ là văn bản để bạn chọn/sao chép; không tự mở trình duyệt hoặc truy cập mạng."
                wrapMode: Text.Wrap
                color: page.uiPalette.muted
            }
        }
    }
    Pane {
        objectName: "aboutLicenseCard"
        Layout.fillWidth: true
        padding: page.uiPalette.cardPadding
        background: Rectangle { color: page.uiPalette.card; radius: page.uiPalette.radius; border.color: page.uiPalette.stroke }
        contentItem: ColumnLayout {
            spacing: 12
            Label {
                Layout.fillWidth: true
                text: "Giấy phép dự án — GNU GPL v3"
                wrapMode: Text.Wrap
                color: page.uiPalette.text
                font.pixelSize: page.uiPalette.headingSize
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                text: "Toàn văn LICENSE của repository được đóng kèm binary, xem được offline. Đây là giấy phép dự án; không thay thế thông báo giấy phép của Qt hoặc các thư viện hệ thống đi kèm."
                wrapMode: Text.Wrap
                color: page.uiPalette.muted
            }
            SettingsButton {
                objectName: "toggleAboutLicense"
                uiPalette: page.uiPalette
                text: page.licenseOpen ? "Ẩn giấy phép" : "Xem giấy phép"
                enabled: page.info !== null
                onClicked: {
                    if (page.licenseOpen) page.info.clearLicense()
                    else page.info.loadLicense()
                }
            }
            Label {
                objectName: "aboutLicenseFeedback"
                Layout.fillWidth: true
                visible: page.info !== null && page.info.licenseState === "error"
                text: "Không đọc được giấy phép đóng kèm; không tải bản thay thế từ mạng."
                wrapMode: Text.Wrap
                color: page.uiPalette.muted
            }
            ScrollView {
                visible: page.licenseOpen
                Layout.fillWidth: true
                Layout.preferredHeight: 260
                contentWidth: availableWidth
                clip: true
                TextArea {
                    objectName: "aboutLicenseText"
                    text: page.info !== null ? page.info.licenseText : ""
                    textFormat: TextEdit.PlainText
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    color: page.uiPalette.text
                    selectionColor: page.uiPalette.accent
                    selectedTextColor: page.uiPalette.accentText
                    background: null
                    Accessible.name: "Toàn văn giấy phép GPL v3 chỉ đọc"
                }
            }
        }
    }
}
