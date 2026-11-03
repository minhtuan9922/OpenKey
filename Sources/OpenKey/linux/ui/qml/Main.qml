import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: root
    objectName: "openkeySettingsWindow"
    visible: true
    width: 1040
    height: 760
    minimumWidth: 720
    minimumHeight: 560
    title: "OpenKey — Ubuntu (UI preview)"
    property int pageIndex: 0
    property string themeMode: "system"
    property bool sidebarExpanded: true
    property var overviewModel: null
    property var configController: null
    readonly property string sampledInput: overviewModel ? overviewModel.inputStatus : "unknown"
    readonly property string serviceState: overviewModel ? overviewModel.serviceState : "disabled"
    readonly property string savedStyle: overviewModel ? overviewModel.inputStyle : "unknown"
    readonly property string configState: overviewModel ? overviewModel.configState : "not-read"
    readonly property string diagnostics: overviewModel ? overviewModel.diagnostics : "unknown"
    readonly property string inputLabel: ({telex: "OpenKey Telex", vni: "OpenKey VNI",
        english: "Tiếng Anh (US)", inactive: "Fcitx5 chưa kích hoạt", other: "Bộ gõ khác",
        unknown: "Chưa xác định", unavailable: "Không đọc được trạng thái Fcitx5"})[sampledInput] || "Chưa xác định"
    readonly property string configLabel: configState === "read" ? "Đã đọc file người dùng" :
        configState === "missing" ? "Chưa có file người dùng" :
        configState === "not-read" ? "Chưa đọc cấu hình" :
        configState === "invalid" ? "Cấu hình không hợp lệ" : "Không đọc được file"
    readonly property string styleLabel: savedStyle === "direct" ? "Không gạch chân (Direct)" :
        savedStyle === "preedit" ? "Có gạch chân (Preedit)" : "Chưa xác định"
    readonly property var pages: [
        {key: "overview", label: "Tổng quan", symbol: "home"},
        {key: "spelling", label: "Chính tả & gõ nhanh", symbol: "spelling"},
        {key: "hotkeys", label: "Phím tắt", symbol: "hotkeys"},
        {key: "compatibility", label: "Ứng dụng & tương thích", symbol: "compatibility"},
        {key: "advanced", label: "Cài đặt nâng cao", symbol: "advanced"},
        {key: "macro", label: "Gõ tắt", symbol: "macro"},
        {key: "convert", label: "Chuyển mã", symbol: "convert"},
        {key: "about", label: "Giới thiệu", symbol: "about"}
    ]
    readonly property var descriptions: [
        "",
        "Các tùy chọn chính tả và gõ nhanh sẽ được nối với cấu hình Linux trong task sau. Chưa thể thay đổi thiết lập tại đây.",
        "Trang cấu hình phím tắt chưa tích hợp backend. Không đăng ký hoặc thay đổi phím tắt hệ thống từ cửa sổ này.",
        "Danh sách ứng dụng và chính sách tương thích chưa tích hợp. Bộ gõ Fcitx5 đang chạy độc lập với giao diện xem trước.",
        "Các tùy chọn nâng cao chưa tích hợp. Trang Tổng quan chỉ đọc hai khóa cấu hình đã lưu; không ghi thay đổi.",
        "Trình quản lý gõ tắt chưa tích hợp. Chưa hỗ trợ thêm, sửa, xóa hoặc nhập bảng gõ tắt từ giao diện này.",
        "Công cụ chuyển mã chưa tích hợp. Nội dung clipboard và tài liệu không được truy cập từ giao diện xem trước.",
        "OpenKey cho Ubuntu — nền tảng giao diện v0.4 đang phát triển, chưa phải bản phát hành hoàn chỉnh.\n\nGiao diện Qt Quick/QML lấy bố cục điều hướng từ phiên bản Windows. Bộ gõ Fcitx5 và dịch vụ tray vẫn hoạt động độc lập."
    ]
    function selectPage(index) {
        if (index < 0 || index >= pages.length || index !== Math.floor(index))
            return false
        pageIndex = index
        return true
    }
    function focusPage(index) {
        if (selectPage(index)) {
            const button = index === 7 ? aboutButton : navRepeater.itemAt(index)
            button.forceActiveFocus(Qt.TabFocusReason)
        }
    }
    SystemPalette { id: systemPalette; colorGroup: SystemPalette.Active }
    UiPalette {
        id: ui
        dark: root.themeMode === "dark" ||
              (root.themeMode === "system" && systemPalette.window.hslLightness < 0.5)
    }
    color: ui.page
    palette.window: ui.page
    palette.windowText: ui.text
    palette.base: ui.card
    palette.text: ui.text
    palette.button: ui.card
    palette.buttonText: ui.text
    palette.dark: ui.text
    palette.mid: ui.stroke
    palette.highlight: ui.accent
    palette.highlightedText: ui.accentText

    RowLayout {
        anchors.fill: parent
        spacing: 0
        Rectangle {
            objectName: "sidebar"
            Layout.preferredWidth: root.sidebarExpanded ? 244 : 72
            Layout.fillHeight: true
            color: ui.sidebar
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    ToolButton {
                        objectName: "sidebarToggle"
                        implicitWidth: 48
                        implicitHeight: 48
                        Accessible.name: root.sidebarExpanded ? "Thu gọn menu" : "Mở rộng menu"
                        contentItem: Item {
                            NavIcon { anchors.centerIn: parent; width: 22; height: 22; symbol: "menu"; ink: ui.text }
                        }
                        onClicked: root.sidebarExpanded = !root.sidebarExpanded
                    }
                    Label {
                        visible: root.sidebarExpanded
                        Layout.fillWidth: true
                        text: "OpenKey"
                        font.pixelSize: 21
                        font.weight: Font.DemiBold
                        color: ui.text
                    }
                }
                Item { Layout.preferredHeight: 8 }
                Repeater {
                    id: navRepeater
                    model: root.pages.slice(0, 7)
                    NavButton {
                        required property var modelData
                        required property int index
                        objectName: "nav-" + modelData.key
                        Layout.fillWidth: true
                        uiPalette: ui
                        text: modelData.label
                        symbol: modelData.symbol
                        expanded: root.sidebarExpanded
                        selected: root.pageIndex === index
                        onClicked: root.selectPage(index)
                        Keys.onDownPressed: root.focusPage(index + 1)
                        Keys.onUpPressed: root.focusPage(Math.max(0, index - 1))
                    }
                }
                Item { Layout.fillHeight: true }
                NavButton {
                    id: aboutButton
                    objectName: "nav-about"
                    Layout.fillWidth: true
                    uiPalette: ui
                    text: root.pages[7].label
                    symbol: "about"
                    expanded: root.sidebarExpanded
                    selected: root.pageIndex === 7
                    onClicked: root.selectPage(7)
                    Keys.onUpPressed: root.focusPage(6)
                    Keys.onDownPressed: root.focusPage(0)
                }
                Label {
                    visible: root.sidebarExpanded
                    Layout.leftMargin: 14
                    Layout.bottomMargin: 8
                    text: "Ubuntu · UI preview"
                    font.pixelSize: 11
                    color: ui.muted
                }
            }
        }
        Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: ui.stroke }
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.margins: ui.pagePadding
            spacing: ui.sectionSpacing
            RowLayout {
                Layout.fillWidth: true
                spacing: 16
                Label {
                    objectName: "pageTitle"
                    Layout.fillWidth: true
                    text: root.pages[root.pageIndex].label
                    wrapMode: Text.WordWrap
                    font.pixelSize: ui.titleSize
                    font.weight: Font.DemiBold
                    color: ui.text
                }
                SettingsComboBox {
                    objectName: "themePicker"
                    uiPalette: ui
                    Layout.preferredWidth: 156
                    model: ["Theo hệ thống", "Sáng", "Tối"]
                    Accessible.name: "Giao diện"
                    currentIndex: ["system", "light", "dark"].indexOf(root.themeMode)
                    onActivated: index => root.themeMode = ["system", "light", "dark"][index]
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Bản xem trước · chỉ đổi chế độ khi nhấn Áp dụng"
                wrapMode: Text.WordWrap
                color: ui.muted
                font.pixelSize: ui.bodySize
            }
            ScrollView {
                id: scroll
                objectName: "contentScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                ColumnLayout {
                    width: scroll.availableWidth
                    spacing: ui.sectionSpacing
                    InfoCard {
                        visible: root.pageIndex === 0
                        Layout.fillWidth: true
                        uiPalette: ui
                        heading: "Giao diện Ubuntu đang phát triển"
                        badge: "Bản xem trước"
                        description: "Trang Tổng quan đọc trạng thái và cấu hình đã lưu. Bạn có thể đổi chế độ gõ bằng nút Áp dụng qua Fcitx5. Các trang thiết lập khác chưa tích hợp; giao diện không tự khởi động bộ gõ."
                    }
                    StyleSettings {
                        visible: root.pageIndex === 0
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                    }
                    GridLayout {
                        visible: root.pageIndex === 0
                        Layout.fillWidth: true
                        columns: scroll.availableWidth >= 640 ? 2 : 1
                        columnSpacing: ui.sectionSpacing
                        rowSpacing: ui.sectionSpacing
                        InfoCard {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 1
                            uiPalette: ui
                            objectName: "inputStatusCard"
                            heading: "Trạng thái bộ gõ"
                            badge: root.serviceState === "connected" ? "Đã lấy mẫu" : "Chưa kết nối"
                            description: root.inputLabel + "\n\n" +
                                (root.serviceState === "connected" ?
                                 "Nguồn: context gần nhất do service quan sát, không phải trạng thái riêng của ô đang gõ. Bảng mã chưa được truy vấn." :
                                 root.serviceState === "disabled" ? "Đang xem trước offline, chưa truy vấn service." :
                                 root.serviceState === "connecting" ? "Đang đọc trạng thái từ service…" :
                                 "Không lấy được phản hồi hợp lệ từ service. Giao diện không tự khởi động service hoặc Fcitx5.")
                        }
                        InfoCard {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.preferredWidth: 1
                            uiPalette: ui
                            objectName: "savedConfigCard"
                            heading: "Chế độ đã lưu"
                            badge: "Chỉ đọc"
                            description: root.styleLabel + "\n" + root.configLabel +
                                "\nDiagnostics: " + (root.diagnostics === "enabled" ? "Bật" : root.diagnostics === "disabled" ? "Tắt" : "Chưa xác định") +
                                "\n\nKhông xác nhận Fcitx5 đã nạp file này. Direct vẫn có thể fallback Preedit tùy ứng dụng."
                        }
                    }
                    InfoCard {
                        visible: root.pageIndex === 0
                        Layout.fillWidth: true
                        uiPalette: ui
                        heading: "Trạng thái tích hợp"
                        badge: "Tích hợp một phần"
                        description: "Fcitx5 và icon tray hoạt động độc lập. Đóng cửa sổ không dừng bộ gõ. Giao diện không ghi trực tiếp file: nút Áp dụng gửi cấu hình chế độ qua Fcitx5 rồi đọc lại runtime. Các thiết lập khác chưa tích hợp."
                    }
                    InfoCard {
                        visible: root.pageIndex !== 0
                        Layout.fillWidth: true
                        uiPalette: ui
                        heading: root.pages[root.pageIndex].label
                        description: root.descriptions[root.pageIndex]
                        badge: root.pageIndex === 7 ? "UI preview" : "Chưa tích hợp"
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Theme chỉ áp dụng cho cửa sổ này, không lưu sau khi thoát."
                wrapMode: Text.WordWrap
                color: ui.muted
                font.pixelSize: ui.smallSize
            }
        }
    }
}
