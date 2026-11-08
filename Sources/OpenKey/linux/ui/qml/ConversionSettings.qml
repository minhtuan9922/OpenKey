import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: page
    objectName: "conversionSettings"
    required property var uiPalette
    property var converter: null
    property var clipboard: null
    property string clipboardMessage: ""
    readonly property var normalizationKeys: ["none", "nfc", "nfd"]
    readonly property var caseKeys: ["keep", "upper", "lower"]
    function invalidate() {
        clipboardMessage = ""
        if (converter !== null) converter.clear()
    }
    function clipboardFeedback(status) {
        return status === "pasted" ? "Đã dán nguồn; chưa chuyển đổi." :
            status === "copied" ? "Đã gửi kết quả dạng văn bản thuần tới clipboard. Xóa ô nhập không xóa clipboard hoặc lịch sử của hệ thống." :
            status === "too-large" ? "Văn bản clipboard vượt giới hạn; không thay đổi nội dung." :
            status === "invalid" ? "Văn bản Unicode không hợp lệ; không thay đổi nội dung." :
            status === "not-text" ? "Clipboard không có văn bản thuần để dán." :
            status === "empty" ? "Không có văn bản để thực hiện thao tác." :
            "Không truy cập được clipboard. Nguồn và clipboard được giữ nguyên."
    }
    padding: uiPalette.cardPadding
    background: Rectangle {
        color: page.uiPalette.card
        radius: page.uiPalette.radius
        border.color: page.uiPalette.stroke
    }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: "Chuyển đổi văn bản Unicode"
            wrapMode: Text.Wrap
            color: page.uiPalette.text
            font.pixelSize: page.uiPalette.headingSize
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            text: "Nhập hoặc dán văn bản, chọn tùy chọn rồi nhấn Chuyển đổi. Xử lý trong bộ nhớ; clipboard chỉ dùng khi bấm nút. Chuyển file và bảng mã VNI/TCVN3/CP1258 có ở các mục bên dưới. Chuyển mã nhanh toàn hệ thống chưa hỗ trợ."
            wrapMode: Text.Wrap
            color: page.uiPalette.muted
            font.pixelSize: page.uiPalette.bodySize
        }
        Label {
            text: "Văn bản nguồn"
            color: page.uiPalette.text
            font.pixelSize: page.uiPalette.bodySize
        }
        SettingsButton {
            objectName: "pasteConversionSource"
            uiPalette: page.uiPalette
            text: "Dán từ clipboard"
            enabled: page.clipboard !== null
            onClicked: {
                const reply = page.clipboard.readText()
                if (reply.ok) {
                    input.text = reply.text
                    page.invalidate()
                }
                page.clipboardMessage = page.clipboardFeedback(reply.status)
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            contentWidth: availableWidth
            clip: true
            background: Rectangle {
                color: page.uiPalette.card
                radius: 5
                border.width: input.activeFocus ? 2 : 1
                border.color: input.activeFocus ? page.uiPalette.accent : page.uiPalette.stroke
            }
            TextArea {
                id: input
                objectName: "conversionInput"
                placeholderText: "Ví dụ: Tiếng Việt — Đặng, cộng hoà"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: page.uiPalette.text
                placeholderTextColor: page.uiPalette.muted
                selectionColor: page.uiPalette.accent
                selectedTextColor: page.uiPalette.accentText
                font.pixelSize: page.uiPalette.bodySize
                padding: 12
                background: null
                Accessible.name: "Văn bản nguồn chuyển đổi Unicode"
                onTextChanged: page.invalidate()
            }
        }
        Label {
            Layout.fillWidth: true
            text: input.text.length + " / 65536 đơn vị UTF-16; đổi nguồn hoặc tùy chọn sẽ hủy tác vụ và xóa kết quả cũ."
            color: page.uiPalette.muted
            wrapMode: Text.Wrap
            font.pixelSize: page.uiPalette.smallSize
        }
        Label { text: "Chuẩn hóa Unicode"; color: page.uiPalette.text }
        SettingsComboBox {
            id: normalization
            objectName: "conversionNormalization"
            Layout.fillWidth: true
            uiPalette: page.uiPalette
            model: ["Giữ dạng Unicode", "NFC — tổ hợp sẵn", "NFD — phân rã"]
            Accessible.name: "Chuẩn hóa Unicode"
            onActivated: page.invalidate()
        }
        Label { text: "Chữ hoa / thường"; color: page.uiPalette.text }
        SettingsComboBox {
            id: letterCase
            objectName: "conversionCase"
            Layout.fillWidth: true
            uiPalette: page.uiPalette
            model: ["Giữ nguyên hoa/thường", "CHỮ HOA", "chữ thường"]
            Accessible.name: "Chuyển chữ hoa hoặc thường"
            onActivated: page.invalidate()
        }
        Label { text: "Dấu tiếng Việt"; color: page.uiPalette.text }
        SettingsComboBox {
            id: removeMarks
            objectName: "conversionMarks"
            Layout.fillWidth: true
            uiPalette: page.uiPalette
            model: ["Giữ dấu", "Bỏ dấu tiếng Việt (đ → d)"]
            Accessible.name: "Giữ hoặc bỏ dấu tiếng Việt"
            onActivated: page.invalidate()
        }
        RowLayout {
            spacing: 12
            SettingsButton {
                objectName: "convertText"
                uiPalette: page.uiPalette
                primary: true
                text: "Chuyển đổi"
                enabled: page.converter !== null && !page.converter.busy && input.text.length > 0 && input.text.length <= 65536
                onClicked: {
                    page.clipboardMessage = ""
                    page.converter.convert(input.text, page.normalizationKeys[normalization.currentIndex],
                        page.caseKeys[letterCase.currentIndex], removeMarks.currentIndex === 1)
                }
            }
            SettingsButton {
                objectName: "cancelConversion"
                uiPalette: page.uiPalette
                text: "Hủy"
                enabled: page.converter !== null && page.converter.state === "running"
                onClicked: page.converter.cancel()
            }
            SettingsButton {
                objectName: "clearConversion"
                uiPalette: page.uiPalette
                text: "Xóa văn bản"
                enabled: input.text.length > 0 || (page.converter !== null && page.converter.result.length > 0)
                onClicked: { input.clear(); page.invalidate() }
            }
        }
        Label {
            objectName: "conversionFeedback"
            Layout.fillWidth: true
            text: input.text.length > 65536 ? "Văn bản vượt giới hạn 65536 đơn vị UTF-16; hãy chia nhỏ trước khi chuyển đổi." :
                page.converter === null ? "Công cụ chuyển đổi chưa sẵn sàng." :
                page.converter.state === "running" ? "Đang chuyển đổi trong nền; có thể Hủy hoặc sửa nguồn/tùy chọn." :
                page.converter.busy ? "Đã bỏ tác vụ cũ; đang chờ worker dừng trước khi chuyển đổi tiếp." :
                page.converter.state === "cancelled" ? "Đã hủy chuyển đổi; nguồn được giữ nguyên." :
                page.converter.state === "converted" ? "Đã chuyển đổi trong bộ nhớ. Chọn kết quả và dùng Ctrl+C nếu muốn sao chép." :
                page.converter.state === "invalid" ? "Tùy chọn hoặc văn bản Unicode không hợp lệ (không nhận NUL hoặc surrogate lẻ)." :
                page.converter.state === "too-large" ? "Văn bản hoặc kết quả vượt giới hạn xử lý." :
                page.converter.state === "empty" ? "Hãy nhập văn bản nguồn." :
                "Chưa chuyển đổi; chọn tùy chọn không tự xử lý văn bản."
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: page.uiPalette.muted
            Accessible.name: text
        }
        Label { text: "Kết quả (chỉ đọc)"; color: page.uiPalette.text }
        SettingsButton {
            objectName: "copyConversionResult"
            uiPalette: page.uiPalette
            text: "Sao chép kết quả"
            enabled: page.clipboard !== null && page.converter !== null &&
                page.converter.state === "converted" && page.converter.result.length > 0
            onClicked: page.clipboardMessage = page.clipboardFeedback(page.clipboard.copyText(page.converter.result))
        }
        Label {
            objectName: "conversionClipboardFeedback"
            Layout.fillWidth: true
            visible: text.length > 0
            text: page.clipboardMessage
            textFormat: Text.PlainText
            wrapMode: Text.Wrap
            color: page.uiPalette.muted
            Accessible.name: text
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            contentWidth: availableWidth
            clip: true
            background: Rectangle {
                color: page.uiPalette.card
                radius: 5
                border.width: output.activeFocus ? 2 : 1
                border.color: output.activeFocus ? page.uiPalette.accent : page.uiPalette.stroke
            }
            TextArea {
                id: output
                objectName: "conversionOutput"
                text: page.converter !== null ? page.converter.result : ""
                readOnly: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: page.uiPalette.text
                selectionColor: page.uiPalette.accent
                selectedTextColor: page.uiPalette.accentText
                font.pixelSize: page.uiPalette.bodySize
                padding: 12
                background: null
                Accessible.name: "Kết quả chuyển đổi Unicode chỉ đọc"
            }
        }
        Label {
            Layout.fillWidth: true
            text: "Bỏ dấu chỉ ánh xạ các chữ cái tiếng Việt sang chữ Latin không dấu; không nhận diện ngôn ngữ hay xóa mọi dấu kết hợp. Ví dụ é cũng thành e trong văn bản tiếng Pháp. Bản nháp không lưu vào preferences; Xóa văn bản xóa nguồn/kết quả nhưng không xóa clipboard hoặc lịch sử clipboard của hệ thống."
            wrapMode: Text.Wrap
            color: page.uiPalette.muted
            font.pixelSize: page.uiPalette.smallSize
        }
    }
}
