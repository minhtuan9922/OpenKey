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
    property var hotkeyOverview: null
    property var uiEnvironment: ({})
    property var textConverter: null
    property var aboutInfo: null
    property var clipboardBridge: null
    property var uiPreferences: null
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
        savedStyle === "commit-only" ? "Gửi cả từ (CommitOnly)" :
        savedStyle === "native-direct" ? "Gõ trực tiếp, không ô nổi (NativeDirect)" :
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
        "Kiểm tra chính tả, đặt dấu kiểu mới và Gõ nhanh Telex được kết nối khi addon hỗ trợ schema. Các tùy chọn phụ âm đầu/cuối và khôi phục từ sai chưa tích hợp.",
        "Hiển thị phím tắt runtime của Fcitx5, chỉ đọc. Không đăng ký hoặc thay đổi phím tắt hệ thống từ cửa sổ này.",
        "Môi trường cửa sổ OpenKey, chế độ gõ chung và hướng dẫn tương thích. Chính sách riêng theo ứng dụng chưa tích hợp.",
        "Chẩn đoán Direct đã hỗ trợ đọc và áp dụng qua Fcitx5. Các tùy chọn nâng cao khác chưa tích hợp.",
        "Trình quản lý gõ tắt chưa tích hợp. Chưa hỗ trợ thêm, sửa, xóa hoặc nhập bảng gõ tắt từ giao diện này.",
        "Công cụ Unicode thủ công: chuẩn hóa, đổi hoa/thường và bỏ dấu tiếng Việt. Chưa hỗ trợ VNI/TCVN3 hoặc tự chuyển clipboard.",
        "OpenKey cho Ubuntu — nền tảng giao diện v0.4 đang phát triển, chưa phải bản phát hành hoàn chỉnh.\n\nGiao diện Qt Quick/QML lấy bố cục điều hướng từ phiên bản Windows. Bộ gõ Fcitx5 và dịch vụ tray vẫn hoạt động độc lập."
    ]
    function selectPage(index) {
        if (index < 0 || index >= pages.length || index !== Math.floor(index))
            return false
        if (pageIndex !== index) {
            pageIndex = index
            // Each page starts at its top; text drafts and runtime values remain intact.
            if (scroll.contentItem) scroll.contentItem.contentY = 0
        }
        return true
    }
    function focusPage(index) {
        if (selectPage(index)) {
            const button = index === 7 ? aboutButton : navRepeater.itemAt(index)
            button.forceActiveFocus(Qt.TabFocusReason)
        }
    }
    function cyclePage(delta) {
        focusPage((pageIndex + delta + pages.length) % pages.length)
    }
    Shortcut {
        sequence: "Ctrl+Tab"
        context: Qt.WindowShortcut
        onActivated: root.cyclePage(1)
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        context: Qt.WindowShortcut
        onActivated: root.cyclePage(-1)
    }
    Shortcut {
        sequence: "F1"
        context: Qt.WindowShortcut
        onActivated: root.focusPage(7)
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
                        id: sidebarToggle
                        objectName: "sidebarToggle"
                        implicitWidth: 48
                        implicitHeight: 48
                        Accessible.name: root.sidebarExpanded ? "Thu gọn menu" : "Mở rộng menu"
                        background: Rectangle {
                            radius: ui.radius
                            color: sidebarToggle.hovered ? ui.hover : "transparent"
                            border.width: sidebarToggle.activeFocus ? 2 : 0
                            border.color: ui.accent
                        }
                        contentItem: Item {
                            NavIcon { anchors.centerIn: parent; width: 22; height: 22; symbol: "menu"; ink: ui.text }
                        }
                        onClicked: {
                            root.sidebarExpanded = !root.sidebarExpanded
                            if (root.uiPreferences !== null)
                                root.uiPreferences.setSidebarExpanded(root.sidebarExpanded)
                        }
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
                    Accessible.role: Accessible.Heading
                    Accessible.name: text
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
                    onActivated: index => {
                        root.themeMode = ["system", "light", "dark"][index]
                        if (root.uiPreferences !== null)
                            root.uiPreferences.setThemeMode(root.themeMode)
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                text: "Bản xem trước · chỉ gửi thay đổi khi nhấn Áp dụng"
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
                        description: "Trang Tổng quan đọc trạng thái và cấu hình đã lưu. Bạn có thể đổi chế độ gõ qua Fcitx5; trang Nâng cao có chẩn đoán Direct, trang Chính tả có hai quy tắc đặt dấu khi addon hỗ trợ. Giao diện không tự khởi động daemon."
                    }
                    CompatibilitySettings {
                        visible: root.pageIndex === 0 || root.pageIndex === 3
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                        environment: root.uiEnvironment
                        showDetails: root.pageIndex === 3
                    }
                    RuntimeStatus {
                        visible: root.pageIndex === 0
                        Layout.fillWidth: true
                        uiPalette: ui
                        overview: root.overviewModel
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
                        description: "Fcitx5 và icon tray hoạt động độc lập. Đóng cửa sổ không dừng bộ gõ. Nút Áp dụng gửi đúng một thiết lập qua Fcitx5 rồi đọc lại runtime. Các trang chưa có backend được ghi rõ."
                    }
                    InfoCard {
                        visible: root.pageIndex === 5
                        Layout.fillWidth: true
                        uiPalette: ui
                        heading: root.pages[root.pageIndex].label
                        description: root.descriptions[root.pageIndex]
                        badge: root.pageIndex === 7 ? "UI preview" : "Chưa tích hợp"
                    }
                    HotkeySettings {
                        visible: root.pageIndex === 2
                        Layout.fillWidth: true
                        uiPalette: ui
                        overview: root.hotkeyOverview
                    }
                    AboutSettings {
                        visible: root.pageIndex === 7
                        Layout.fillWidth: true
                        uiPalette: ui
                        info: root.aboutInfo
                    }
                    ConversionSettings {
                        visible: root.pageIndex === 6
                        Layout.fillWidth: true
                        uiPalette: ui
                        converter: root.textConverter
                        clipboard: root.clipboardBridge
                    }
                    DiagnosticsSettings {
                        visible: root.pageIndex === 4
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                    }
                    PreferencesBackup {
                        visible: root.pageIndex === 4
                        Layout.fillWidth: true
                        uiPalette: ui
                        preferences: root.uiPreferences
                        currentTheme: root.themeMode
                        currentSidebarExpanded: root.sidebarExpanded
                        onRestored: (theme, sidebarExpanded) => {
                            root.themeMode = theme
                            root.sidebarExpanded = sidebarExpanded
                        }
                    }
                    SpellingOption {
                        visible: root.pageIndex === 1
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                        settingKey: "CheckSpelling"
                        heading: "Kiểm tra chính tả khi đặt dấu"
                        description: "Kiểm tra âm tiết trong quá trình bộ gõ xử lý phím và đặt dấu. Đây không phải trình soát lỗi hay gạch đỏ cho toàn bộ tài liệu trong editor."
                    }
                    SpellingOption {
                        visible: root.pageIndex === 1
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                        settingKey: "UseModernOrthography"
                        heading: "Đặt dấu kiểu mới"
                        description: "Bật: hoas/hoa1 → hoá; tắt: hoas/hoa1 → hóa. Chỉ áp dụng khi đã gõ xong từ vì thay đổi kết thúc context hiện tại."
                    }
                    SpellingOption {
                        visible: root.pageIndex === 1
                        Layout.fillWidth: true
                        uiPalette: ui
                        controller: root.configController
                        settingKey: "QuickTelex"
                        heading: "Gõ nhanh Telex"
                        description: "Mặc định tắt. Bật để mở rộng cc → ch, gg → gi, kk → kh, nn → ng, qq → qu, pp → ph, tt → th khi gõ Telex. Không áp dụng cho VNI. Chỉ nhấn Áp dụng khi đã gõ xong từ; đây không phải gõ tắt/macro hay gõ nhanh phụ âm đầu/cuối."
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                objectName: "uiPreferencesFeedback"
                text: root.uiPreferences === null ? "Xem trước: theme/sidebar chỉ giữ trong phiên chạy." :
                    root.uiPreferences.status === "error" ? "Không lưu được giao diện; thiết lập trong phiên có thể chưa được lưu." :
                    root.uiPreferences.status === "invalid" ? "File giao diện không hợp lệ/không hỗ trợ; không ghi đè, lựa chọn mới chỉ giữ trong phiên chạy." :
                    "Theme và sidebar được lưu riêng cho giao diện, không đổi cấu hình bộ gõ."
                wrapMode: Text.WordWrap
                color: ui.muted
                font.pixelSize: ui.smallSize
            }
        }
    }
}
