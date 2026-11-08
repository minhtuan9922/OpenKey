import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

Pane {
    id: card
    required property var uiPalette
    required property string settingKey
    required property string heading
    required property string description
    property var controller: null
    readonly property string prefix: settingKey === "MacrosEnabled" ? "macrosEnabled" : settingKey === "QuickTelex" ? "quickTelex" :
        settingKey === "CheckSpelling" ? "checkSpelling" : settingKey === "UseModernOrthography" ? "modernOrthography" : settingKey.charAt(0).toLowerCase() + settingKey.slice(1)
    objectName: prefix + "Settings"
    readonly property bool ready: controller !== null && controller.ready
    readonly property bool busy: controller !== null && controller.busy
    readonly property bool supported: settingSupported()
    function settingSupported() {
        if (controller === null) return false
        if (settingKey === "MacrosEnabled") return controller.macrosSupported
        if (settingKey === "QuickTelex") return controller.quickTelexSupported
        if (settingKey === "CheckSpelling" || settingKey === "UseModernOrthography") return controller.spellingSupported
        return controller.runtimeAdvanced[settingKey] !== undefined
    }
    function runtimeValue() {
        if (controller === null) return ""
        if (settingKey === "MacrosEnabled") return controller.runtimeMacrosEnabled
        if (settingKey === "QuickTelex") return controller.runtimeQuickTelex
        if (settingKey === "CheckSpelling") return controller.runtimeCheckSpelling
        if (settingKey === "UseModernOrthography") return controller.runtimeModernOrthography
        return controller.runtimeAdvanced[settingKey] || ""
    }
    readonly property string value: runtimeValue()
    ConfigFeedback { id: feedback; result: card.controller !== null ? card.controller.result : "offline" }
    function syncPicker() {
        if (!picker) return
        // Read the controller directly: the changed signal can arrive before
        // QML has reevaluated the derived ready/supported bindings.
        if (controller !== null && controller.ready &&
            settingSupported())
            picker.currentIndex = runtimeValue() === "True" ? 1 : 0
        else if (controller === null || !controller.busy)
            picker.currentIndex = -1
    }
    Component.onCompleted: syncPicker()
    onControllerChanged: syncPicker()
    Connections {
        target: card.controller
        function onChanged() { card.syncPicker() }
    }
    padding: uiPalette.cardPadding
    background: Rectangle { color: card.uiPalette.card; radius: card.uiPalette.radius; border.color: card.uiPalette.stroke }
    contentItem: ColumnLayout {
        spacing: 12
        Label {
            Layout.fillWidth: true
            text: card.heading
            wrapMode: Text.Wrap
            color: card.uiPalette.text
            font.pixelSize: card.uiPalette.headingSize
            font.weight: Font.DemiBold
        }
        Label {
            Layout.fillWidth: true
            text: card.description
            wrapMode: Text.Wrap
            color: card.uiPalette.muted
            font.pixelSize: card.uiPalette.bodySize
        }
        Label {
            objectName: card.prefix + "RuntimeLabel"
            text: "Runtime: " + (card.value === "True" ? "Bật" : card.value === "False" ? "Tắt" : "Chưa xác định")
            color: card.uiPalette.text
        }
        SettingsComboBox {
            id: picker
            objectName: card.prefix + "Picker"
            Layout.fillWidth: true
            uiPalette: card.uiPalette
            model: ["Tắt", "Bật"]
            currentIndex: -1
            displayText: currentIndex < 0 ? "Chưa đọc được thiết lập tương thích" : currentText
            enabled: card.ready && card.supported && !card.busy
            opacity: enabled ? 1 : 0.65
            Accessible.name: card.heading
            background: Rectangle {
                implicitHeight: 40
                radius: 5
                color: card.uiPalette.card
                border.width: picker.visualFocus ? 2 : 1
                border.color: picker.visualFocus ? card.uiPalette.accent : card.uiPalette.stroke
            }
        }
        RowLayout {
            spacing: 12
            SettingsButton {
                objectName: "apply" + card.settingKey
                uiPalette: card.uiPalette
                primary: true
                text: "Áp dụng"
                enabled: card.ready && card.supported && !card.busy && picker.currentIndex >= 0 &&
                    (picker.currentIndex === 1 ? "True" : "False") !== card.value
                onClicked: {
                    const target = picker.currentIndex === 1 ? "True" : "False"
                    if (card.settingKey === "MacrosEnabled") card.controller.applyMacrosEnabled(target)
                    else if (card.settingKey === "CheckSpelling") card.controller.applySpelling(target)
                    else if (card.settingKey === "UseModernOrthography") card.controller.applyModernOrthography(target)
                    else if (card.settingKey === "QuickTelex") card.controller.applyQuickTelex(target)
                    else card.controller.applyAdvanced(card.settingKey, target)
                }
            }
            SettingsButton {
                objectName: "reload" + card.settingKey
                uiPalette: card.uiPalette
                text: "Đọc lại"
                enabled: card.controller !== null && !card.busy
                onClicked: card.controller.reload()
            }
        }
        Label {
            objectName: card.prefix + "Feedback"
            Layout.fillWidth: true
            text: card.ready && !card.supported ? (card.settingKey === "QuickTelex" ?
                "Addon đang chạy chưa hỗ trợ Gõ nhanh Telex. Cần alpha.11 hoặc schema QuickTelex tương thích; tùy chọn này bị khóa." :
                card.settingKey === "CheckSpelling" || card.settingKey === "UseModernOrthography" ?
                "Addon đang chạy chưa có schema chính tả tương thích. Cần alpha.10 hoặc bản mới hỗ trợ hai khóa; các điều khiển này bị khóa." :
                "Addon đang chạy chưa hỗ trợ tùy chọn này. Cài " + (card.settingKey === "UseGnomeApplicationIdentity" ? "alpha.52" : card.settingKey === "RememberInputMethodPerApplication" ? "alpha.51" : "alpha.50") + " hoặc bản mới có schema tương thích, rồi nạp lại addon để sử dụng.") : feedback.text
            wrapMode: Text.Wrap
            textFormat: Text.PlainText
            color: card.uiPalette.muted
            Accessible.name: text
        }
    }
}
