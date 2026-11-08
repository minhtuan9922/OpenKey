import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: page
    objectName: "legacyConversion"
    required property var uiPalette
    property var converter: null
    property var clipboard: null
    property string message: ""
    readonly property var encodingKeys: ["unicode", "tcvn3", "vni", "compound", "cp1258"]
    readonly property var encodingLabels: ["Unicode dựng sẵn", "TCVN3 (ABC)", "VNI Windows", "Unicode tổ hợp", "Vietnamese CP1258"]
    function invalidate() { message = ""; if (converter !== null) converter.clear() }
    padding: uiPalette.cardPadding
    background: Rectangle { color: page.uiPalette.card; radius: page.uiPalette.radius; border.color: page.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            text: "Chuyển mã VNI / TCVN3"
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: page.uiPalette.text
            font.pixelSize: page.uiPalette.headingSize
            font.weight: Font.DemiBold
        }
        Label {
            text: "Chọn đúng bảng mã nguồn và đích rồi nhấn Chuyển mã. Không tự nhận diện bảng mã hoặc đổi font. TCVN3 có thể không phân biệt đầy đủ chữ hoa/thường do dùng chung ký tự; kiểm tra kết quả trước khi sử dụng."
            color: page.uiPalette.muted
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }
        Label { text: "Bảng mã nguồn"; color: page.uiPalette.text }
        SettingsComboBox {
            id: source
            objectName: "legacySourceEncoding"
            uiPalette: page.uiPalette
            Layout.fillWidth: true
            model: page.encodingLabels
            Accessible.name: "Bảng mã nguồn"
            onActivated: page.invalidate()
        }
        Label { text: "Bảng mã đích"; color: page.uiPalette.text }
        SettingsComboBox {
            id: target
            objectName: "legacyTargetEncoding"
            uiPalette: page.uiPalette
            Layout.fillWidth: true
            model: page.encodingLabels
            currentIndex: 1
            Accessible.name: "Bảng mã đích"
            onActivated: page.invalidate()
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            contentWidth: availableWidth
            clip: true
            TextArea {
                id: input
                objectName: "legacyConversionSource"
                Accessible.name: "Văn bản nguồn chuyển mã cũ"
                placeholderText: "Nhập hoặc dán văn bản cần chuyển mã…"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                padding: 12
                color: page.uiPalette.text
                placeholderTextColor: page.uiPalette.muted
                selectionColor: page.uiPalette.accent
                selectedTextColor: page.uiPalette.accentText
                background: Rectangle { color: page.uiPalette.page; radius: 5; border.color: input.activeFocus ? page.uiPalette.accent : page.uiPalette.stroke }
                onTextChanged: page.invalidate()
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "convertLegacyText"
                uiPalette: page.uiPalette
                text: "Chuyển mã"
                primary: true
                enabled: page.converter !== null && !page.converter.busy && input.length > 0 && input.length <= 65536
                onClicked: page.converter.convertLegacy(input.text, page.encodingKeys[source.currentIndex], page.encodingKeys[target.currentIndex])
            }
            SettingsButton {
                objectName: "cancelLegacyConversion"
                uiPalette: page.uiPalette
                text: "Hủy"
                enabled: page.converter !== null && page.converter.state === "running"
                onClicked: page.converter.cancel()
            }
            SettingsButton {
                objectName: "clearLegacyConversion"
                uiPalette: page.uiPalette
                text: "Xóa văn bản"
                enabled: input.length > 0 || (page.converter !== null && page.converter.result.length > 0)
                onClicked: { input.clear(); page.invalidate() }
            }
        }
        Label {
            objectName: "legacyConversionFeedback"
            text: page.message.length > 0 ? page.message : input.length > 65536 ? "Nguồn vượt giới hạn 65536 đơn vị UTF-16." :
                page.converter === null ? "Công cụ chuyển mã chưa sẵn sàng." :
                page.converter.state === "running" ? "Đang chuyển mã; có thể Hủy hoặc sửa nguồn." :
                page.converter.busy ? "Đang chờ tác vụ cũ dừng." :
                page.converter.state === "converted" ? "Đã chuyển mã. Chọn kết quả để sao chép hoặc dùng nút bên dưới." :
                page.converter.state === "cancelled" ? "Đã hủy; giữ nguyên nguồn." :
                page.converter.state === "too-large" ? "Văn bản hoặc kết quả vượt giới hạn." :
                page.converter.state === "invalid" ? "Bảng mã hoặc văn bản không hợp lệ." : "Chưa chuyển mã."
            textFormat: Text.PlainText
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: page.uiPalette.muted
        }
        SettingsButton {
            objectName: "copyLegacyResult"
            uiPalette: page.uiPalette
            text: "Sao chép kết quả"
            enabled: page.clipboard !== null && page.converter !== null && page.converter.state === "converted" && page.converter.result.length > 0
            onClicked: {
                const status = page.clipboard.copyText(page.converter.result)
                page.message = status === "copied" ? "Đã sao chép kết quả." : "Không sao chép được; giữ nguyên kết quả để chọn thủ công."
            }
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            contentWidth: availableWidth
            clip: true
            TextArea {
                objectName: "legacyConversionResult"
                Accessible.name: "Kết quả chuyển mã cũ"
                readOnly: true
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                text: page.converter !== null ? page.converter.result : ""
                padding: 12
                color: page.uiPalette.text
                selectionColor: page.uiPalette.accent
                selectedTextColor: page.uiPalette.accentText
                background: Rectangle { color: page.uiPalette.page; radius: 5; border.color: page.uiPalette.stroke }
            }
        }
    }
}
