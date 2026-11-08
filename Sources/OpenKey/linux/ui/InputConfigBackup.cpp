#include "InputConfigBackup.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace {
const QStringList keys{"InputStyle", "DirectDiagnostics", "CheckSpelling", "UseModernOrthography", "QuickTelex", "FreeMark", "AllowConsonantZFWJ", "QuickStartConsonant", "QuickEndConsonant", "RestoreWrongSpelling", "RememberInputMethodPerApplication", "UseGnomeApplicationIdentity"};
bool validStyle(const QString& value) {
    return QStringList{"Preedit", "Direct", "CommitOnly", "NativeDirect"}.contains(value);
}
}

QString InputConfigBackup::encode(const QVariantMap& runtime) {
    QJsonObject settings;
    for (const auto& key : keys) {
        if (!runtime.contains(key)) continue;
        const auto value = runtime.value(key);
        if (value.metaType().id() != QMetaType::QString) return {};
        if (key == "InputStyle") {
            if (!validStyle(value.toString())) return {};
            settings[key] = value.toString();
        } else {
            if (value.toString() != "True" && value.toString() != "False") return {};
            settings[key] = value.toString() == "True";
        }
    }
    const auto json = QString::fromUtf8(QJsonDocument(QJsonObject{
        {"format", "openkey-input-settings"}, {"version", 1}, {"settings", settings}
    }).toJson(QJsonDocument::Indented));
    QVariantMap check;
    return decode(json, check) ? json : QString();
}

bool InputConfigBackup::decode(const QString& json, QVariantMap& patch) {
    patch.clear();
    if (json.isEmpty() || json.size() > 4096) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return false;
    const auto root = document.object();
    if (root.size() != 3 || root.value("format") != QJsonValue("openkey-input-settings") ||
        !root.value("version").isDouble() || root.value("version").toDouble() != 1 ||
        !root.value("settings").isObject()) return false;
    const auto settings = root.value("settings").toObject();
    if (!settings.contains("InputStyle") || !settings.contains("DirectDiagnostics") ||
        settings.contains("CheckSpelling") != settings.contains("UseModernOrthography")) return false;
    QVariantMap result;
    for (auto it = settings.begin(); it != settings.end(); ++it) {
        if (!keys.contains(it.key())) return false;
        if (it.key() == "InputStyle") {
            if (!it.value().isString() || !validStyle(it.value().toString())) return false;
            result[it.key()] = it.value().toString();
        } else {
            if (!it.value().isBool()) return false;
            result[it.key()] = it.value().toBool() ? "True" : "False";
        }
    }
    patch = result;
    return true;
}

bool InputConfigBackup::supported(const QVariantMap& patch, const QVariantMap& runtime) {
    for (auto it = patch.cbegin(); it != patch.cend(); ++it) {
        if (!runtime.contains(it.key())) return false;
    }
    const auto style = patch.value("InputStyle").toString();
    return (style != "CommitOnly" || runtime.value("CommitOnlySupported").toBool()) &&
        (style != "NativeDirect" || runtime.value("NativeDirectSupported").toBool());
}
