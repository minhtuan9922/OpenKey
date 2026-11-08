import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
Pane {
    id: card
    objectName: "applicationSettings"
    required property var uiPalette
    property var controller: null
    property var rulesModel: null
    property string rulesBaseline: ""
    property bool rulesLoaded: false
    readonly property bool connected: controller !== null && controller.ready && controller.applicationRulesSupported && !controller.busy
    padding: 20
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label { text: "Chế độ theo ứng dụng"; color: card.uiPalette.text; font.pixelSize: 20; font.bold: true }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: "Khớp chính xác program ID do frontend Fcitx5 cung cấp, ví dụ gtk3-im:google-chrome. Không dùng tên cửa sổ hoặc suy đoán ứng dụng. Bridge chỉ báo gnome-shell/unknown không hỗ trợ quy tắc riêng; các ô đó tiếp tục dùng chế độ chung."
        }
        SettingsButton {
            objectName: "loadApplicationRules"
            uiPalette: card.uiPalette; text: "Đọc quy tắc runtime (thay nháp)"
            enabled: card.connected && card.rulesModel !== null
            onClicked: {
                if (card.rulesModel.load(card.controller.runtimeApplicationRules)) {
                    card.rulesBaseline = card.controller.runtimeApplicationRules
                    card.rulesLoaded = true
                }
            }
        }
        Repeater {
            model: card.rulesModel !== null ? card.rulesModel.entries : []
            delegate: RowLayout {
                required property var modelData
                Layout.fillWidth: true
                Label { Layout.fillWidth: true; text: modelData.key + " → " + modelData.value; textFormat: Text.PlainText; wrapMode: Text.Wrap; color: card.uiPalette.text }
                SettingsButton { uiPalette: card.uiPalette; text: "Xóa"; onClicked: card.rulesModel.remove(modelData.key) }
            }
        }
        SettingsTextField {
            uiPalette: card.uiPalette
            id: program
            objectName: "applicationProgram"
            Layout.fillWidth: true
            maximumLength: 128
            placeholderText: "Program ID chính xác của Fcitx5"
            Accessible.name: "Định danh ứng dụng"
            color: card.uiPalette.text
        }
        SettingsComboBox {
            id: style
            objectName: "applicationStyle"
            uiPalette: card.uiPalette
            Layout.fillWidth: true
            model: ["Preedit", "Direct", "CommitOnly", "NativeDirect"]
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton {
                objectName: "putApplicationRule"
                uiPalette: card.uiPalette; text: "Thêm / thay quy tắc nháp"
                enabled: card.rulesModel !== null && program.length > 0
                onClicked: card.rulesModel.put(program.text, style.currentText)
            }
            SettingsButton {
                objectName: "applyApplicationRules"
                uiPalette: card.uiPalette; text: "Áp dụng quy tắc"; primary: true
                enabled: card.connected && card.rulesModel !== null && card.rulesLoaded && card.rulesBaseline === card.controller.runtimeApplicationRules && card.rulesModel.encoded !== card.rulesBaseline
                onClicked: card.controller.applyApplicationRules(card.rulesModel.encoded)
            }
        }
        ConfigFeedback { id: feedback; result: card.controller !== null ? card.controller.result : "offline" }
        Label {
            Layout.fillWidth: true; wrapMode: Text.Wrap; color: card.uiPalette.muted
            text: card.rulesModel !== null && card.rulesModel.status === "invalid" ? "Quy tắc không hợp lệ; giữ bản nháp trước." :
                card.rulesLoaded && card.connected && card.rulesBaseline !== card.controller.runtimeApplicationRules ? "Quy tắc runtime đã đổi. Đọc lại và đối chiếu trước khi sửa tiếp." : feedback.text
        }
        ScrollView {
            Layout.fillWidth: true; Layout.preferredHeight: 130
            SettingsTextArea {
                uiPalette: card.uiPalette; id: json; objectName: "applicationRulesJson"; textFormat: TextEdit.PlainText; selectByMouse: true; wrapMode: TextEdit.Wrap; placeholderText: "JSON sao lưu quy tắc ứng dụng"; color: card.uiPalette.text }
        }
        Flow {
            Layout.fillWidth: true; spacing: 8
            SettingsButton { uiPalette: card.uiPalette; text: "Xuất JSON nháp"; enabled: card.rulesModel !== null; onClicked: json.text = card.rulesModel.exportJson() }
            SettingsButton { uiPalette: card.uiPalette; text: "Nhập JSON vào nháp"; enabled: card.rulesModel !== null && json.length > 0; onClicked: card.rulesModel.importJson(json.text) }
        }
    }
}
