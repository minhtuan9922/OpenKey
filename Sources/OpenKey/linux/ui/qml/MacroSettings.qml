import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: card
    objectName: "macroSettings"
    required property var uiPalette
    property var controller: null
    property var macroDraft: null
    property var fileAccess: null
    property string fileStatus: ""
    function loadFile(url) {
        if (fileAccess === null) return
        const result = fileAccess.loadMacros(url)
        fileStatus = result.ok ? "Đã mở JSON vào ô nháp; nhấn Nhập JSON để thay bảng nháp." : "Không mở được bảng JSON hợp lệ."
        if (result.ok) backup.text = result.text
    }
    function saveFile(url) {
        if (fileAccess === null) return
        fileStatus = fileAccess.saveMacros(url, backup.text) === "saved" ? "Đã lưu JSON." : "Không lưu được JSON; kiểm tra dữ liệu và quyền ghi."
    }
    FileDialog {
        id: openDialog
        title: "Mở bảng gõ tắt JSON"
        nameFilters: ["JSON (*.json)"]
        onAccepted: card.loadFile(selectedFile)
    }
    FileDialog {
        id: saveDialog
        title: "Lưu bảng gõ tắt JSON"
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: ["JSON (*.json)"]
        onAccepted: card.saveFile(selectedFile)
    }
    property string macroBaseline: ""
    property bool loaded: false
    readonly property bool available: controller !== null && controller.ready && controller.macrosSupported && !controller.busy
    padding: 20
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Gõ tắt"; color: card.uiPalette.text; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            text: "Khớp chính xác từ đang hiển thị, phân biệt hoa/thường; mở rộng khi nhấn Space hoặc Enter. Ví dụ: vn → Việt Nam. Bản nháp chỉ có hiệu lực sau Áp dụng bảng. Chế độ trực tiếp cần ứng dụng xác nhận vùng chữ; nếu chưa xác nhận, giữ nguyên từ."
        }
        SpellingOption {
            Layout.fillWidth: true
            uiPalette: card.uiPalette
            controller: card.controller
            settingKey: "MacrosEnabled"
            heading: "Bật gõ tắt"
            description: "Mặc định tắt. Bảng gõ tắt được lưu riêng với công tắc bật/tắt."
        }
        SettingsButton {
            objectName: "loadMacros"
            uiPalette: card.uiPalette
            text: "Đọc bảng runtime (thay bản nháp)"
            enabled: card.available && card.macroDraft !== null
            onClicked: {
                if (card.macroDraft.load(card.controller.runtimeMacroTable)) {
                    card.macroBaseline = card.controller.runtimeMacroTable
                    card.loaded = true
                }
            }
        }
        Label {
            Layout.fillWidth: true
            color: card.uiPalette.muted
            wrapMode: Text.Wrap
            text: !card.available ? "Cần addon có hỗ trợ gõ tắt và kết nối Fcitx5." :
                card.loaded && card.macroBaseline !== card.controller.runtimeMacroTable ? "Bảng runtime đã đổi. Đọc lại và đối chiếu trước khi áp dụng." :
                "Thêm cùng từ khóa sẽ thay nội dung của mục đó. Tối đa 256 mục; từ khóa tối đa 32 đơn vị UTF-16."
        }
        Repeater {
            model: card.macroDraft !== null ? card.macroDraft.entries : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    text: modelData.key + " → " + modelData.value
                    textFormat: Text.PlainText
                    wrapMode: Text.Wrap
                    color: card.uiPalette.text
                }
                SettingsButton {
                    uiPalette: card.uiPalette; text: "Sửa"
                    onClicked: { keyInput.text = modelData.key; valueInput.text = modelData.value }
                }
                SettingsButton {
                    uiPalette: card.uiPalette; text: "Xóa"
                    onClicked: card.macroDraft.remove(modelData.key)
                }
            }
        }
        SettingsTextField {
            uiPalette: card.uiPalette
            id: keyInput
            objectName: "macroKey"
            Layout.fillWidth: true
            maximumLength: 32
            placeholderText: "Từ khóa, ví dụ vn"
            Accessible.name: "Từ khóa gõ tắt"
            color: card.uiPalette.text
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            SettingsTextArea {
                uiPalette: card.uiPalette
                id: valueInput
                objectName: "macroValue"
                placeholderText: "Nội dung thay thế, ví dụ Việt Nam"
                Accessible.name: "Nội dung gõ tắt"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                color: card.uiPalette.text
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "putMacro"
                uiPalette: card.uiPalette; text: "Thêm / cập nhật bản nháp"
                enabled: card.macroDraft !== null && keyInput.length > 0 && valueInput.length > 0
                onClicked: {
                    if (card.macroDraft.put(keyInput.text, valueInput.text)) { keyInput.clear(); valueInput.clear() }
                }
            }
            SettingsButton {
                objectName: "applyMacros"
                uiPalette: card.uiPalette; text: "Áp dụng bảng"; primary: true
                enabled: card.available && card.macroDraft !== null && card.loaded && card.macroBaseline === card.controller.runtimeMacroTable && card.macroDraft.encoded !== card.macroBaseline
                onClicked: card.controller.applyMacroTable(card.macroDraft.encoded)
            }
        }
        Label { Layout.fillWidth: true; text: card.fileStatus; wrapMode: Text.Wrap; color: card.uiPalette.muted }
        ConfigFeedback { id: feedback; result: card.controller !== null ? card.controller.result : "offline" }
        Label { Layout.fillWidth: true; text: feedback.text; wrapMode: Text.Wrap; color: card.uiPalette.muted }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.macroDraft !== null && card.macroDraft.status === "invalid" ? "Bảng không hợp lệ hoặc vượt giới hạn; bản nháp trước vẫn được giữ." : "JSON chỉ chứa bảng gõ tắt. Nhập JSON thay bản nháp, không tự áp dụng hoặc bật gõ tắt."
        }
        ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            SettingsTextArea {
                uiPalette: card.uiPalette
                id: backup
                objectName: "macroJson"
                Accessible.name: "JSON bảng gõ tắt"
                placeholderText: "JSON nhập / xuất bảng gõ tắt"
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
                selectByMouse: true
                color: card.uiPalette.text
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: 8
            SettingsButton {
                objectName: "exportMacros"
                uiPalette: card.uiPalette; text: "Xuất JSON từ bản nháp"
                enabled: card.macroDraft !== null
                onClicked: backup.text = card.macroDraft.exportJson()
            }
            SettingsButton {
                objectName: "importMacros"
                uiPalette: card.uiPalette; text: "Nhập JSON vào bản nháp"
                enabled: card.macroDraft !== null && backup.length > 0
                onClicked: card.macroDraft.importJson(backup.text)
            }
            SettingsButton {
                objectName: "openMacrosFile"
                uiPalette: card.uiPalette; text: "Mở file JSON…"
                enabled: card.fileAccess !== null
                onClicked: openDialog.open()
            }
            SettingsButton {
                objectName: "saveMacrosFile"
                uiPalette: card.uiPalette; text: "Lưu file JSON…"
                enabled: card.fileAccess !== null && card.fileAccess.validMacroBackup(backup.text)
                onClicked: saveDialog.open()
            }
        }
    }
}
