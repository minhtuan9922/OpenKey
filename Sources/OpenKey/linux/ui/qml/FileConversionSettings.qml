import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs
Pane {
    id: card
    objectName: "fileConversionSettings"
    required property var uiPalette
    property var converter: null
    property url inputFile
    property url outputFile
    readonly property bool busy: converter !== null && converter.busy
    readonly property var keys: ["unicode", "compound", "vni", "tcvn3", "cp1258"]
    FileDialog { id: sourceDialog; title: "Mở file văn bản UTF-8"; onAccepted: card.inputFile = selectedFile }
    FileDialog { id: targetDialog; title: "Lưu file kết quả UTF-8"; fileMode: FileDialog.SaveFile; defaultSuffix: "txt"; onAccepted: card.outputFile = selectedFile }
    padding: 20
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Chuyển đổi file văn bản"; font.pixelSize: 20; font.bold: true; color: card.uiPalette.text }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Đọc và lưu file UTF-8, tối đa 16 MiB nguồn / 64 MiB kết quả. VNI/TCVN3/CP1258 là các ký tự đang được biểu diễn trong UTF-8; chưa đọc file byte legacy hoặc tài liệu Word. Chuyển đổi chạy trong tiến trình riêng, tự dừng sau 30 giây. Hủy sẽ kết thúc tiến trình; kết quả chỉ công bố sau khi hoàn thành."
        }
        SettingsButton { objectName: "chooseConversionInput"; uiPalette: card.uiPalette; text: "Chọn file nguồn…"; enabled: !card.busy && card.converter !== null; onClicked: sourceDialog.open() }
        Label { Layout.fillWidth: true; text: card.inputFile.toString() || "Chưa chọn file nguồn"; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: card.uiPalette.text }
        SettingsButton { objectName: "chooseConversionOutput"; uiPalette: card.uiPalette; text: "Chọn file kết quả…"; enabled: !card.busy && card.converter !== null; onClicked: targetDialog.open() }
        Label { Layout.fillWidth: true; text: card.outputFile.toString() || "Chưa chọn file kết quả"; textFormat: Text.PlainText; wrapMode: Text.WrapAnywhere; color: card.uiPalette.text }
        Label { text: "Bảng mã nguồn → đích"; color: card.uiPalette.muted }
        SettingsComboBox { id: source; objectName: "fileConversionSource"; uiPalette: card.uiPalette; Layout.fillWidth: true; model: ["Unicode dựng sẵn", "Unicode tổ hợp", "VNI Windows", "TCVN3", "Vietnamese CP1258"]; enabled: !card.busy }
        SettingsComboBox { id: target; objectName: "fileConversionTarget"; uiPalette: card.uiPalette; Layout.fillWidth: true; model: source.model; enabled: !card.busy }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton {
                objectName: "startFileConversion"; uiPalette: card.uiPalette; text: "Chuyển đổi và lưu"; primary: true
                enabled: card.converter !== null && !card.busy && card.inputFile.toString().length > 0 && card.outputFile.toString().length > 0
                onClicked: card.converter.start(card.inputFile, card.outputFile, card.keys[source.currentIndex], card.keys[target.currentIndex])
            }
            SettingsButton { objectName: "cancelFileConversion"; uiPalette: card.uiPalette; text: "Hủy"; enabled: card.busy; onClicked: card.converter.cancel() }
        }
        Label {
            objectName: "fileConversionFeedback"
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.converter === null ? "Xem trước: không đọc/ghi file." : ({idle: "Chưa chuyển đổi.", running: "Đang chuyển đổi trong tiến trình riêng…", cancelling: "Đang dừng tiến trình…",
                cancelled: "Đã hủy; không công bố file kết quả.", timeout: "Đã dừng do quá 30 giây; không công bố kết quả.", converted: "Đã lưu file kết quả.",
                invalid: "Đường dẫn hoặc bảng mã không hợp lệ; chọn file kết quả khác file nguồn.", "invalid-input": "Nguồn không phải file thường hợp lệ hoặc lớn hơn 16 MiB.",
                "invalid-output": "Đích không phải file thường hợp lệ.", unwritable: "Không ghi được thư mục/file đích.", conflict: "File đích đã đổi trong lúc xử lý; giữ file đó và bỏ kết quả tạm.",
                failed: "Không chuyển đổi được; kiểm tra UTF-8, giới hạn và quyền đọc/ghi."
            })[card.converter.state] || "Chưa xác định"
        }
    }
}
