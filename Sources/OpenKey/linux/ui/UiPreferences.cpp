#include "UiPreferences.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStringList>
#include <utility>

namespace {
bool validTheme(const QString& theme) { return theme == "system" || theme == "light" || theme == "dark"; }
bool unsafeFile(const QFileInfo& file) {
    return file.isSymLink() || (file.exists() && (!file.isFile() || file.size() > 16384));
}
struct Backup {
    QString status = "invalid", theme;
    bool expanded = true;
};
Backup parseBackup(const QString& json) {
    if (json.size() > 4096) return {"too-large", {}, true};
    // Reject text that UTF-8 conversion would replace or that can hide a NUL.
    for (int i = 0; i < json.size(); ++i) {
        const auto ch = json.at(i);
        if (ch.isNull() || ch.isLowSurrogate()) return {};
        if (ch.isHighSurrogate()) {
            if (++i == json.size() || !json.at(i).isLowSurrogate()) return {};
        }
    }
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) return {};
    const auto object = document.object();
    if (object.size() != 4 || !object.value("format").isString() ||
        object.value("format").toString() != "openkey-ui" ||
        !object.value("version").isDouble() || object.value("version").toDouble() != 1 ||
        !object.value("theme").isString() || !validTheme(object.value("theme").toString()) ||
        !object.value("sidebarExpanded").isBool()) return {};
    return {"ready", object.value("theme").toString(), object.value("sidebarExpanded").toBool()};
}
}
UiPreferences::UiPreferences(QString file, QObject* parent) : QObject(parent), file_(std::move(file)) {}
void UiPreferences::load() {
    loadInternal(true);
}
void UiPreferences::loadInternal(bool notify) {
    theme_ = "system";
    expanded_ = true;
    blocked_ = false;
    const QFileInfo file(file_);
    if (unsafeFile(file)) { blocked_ = true; status_ = "invalid"; }
    else if (!file.exists()) status_ = "missing";
    else {
        QFile readable(file_);
        if (!readable.open(QIODevice::ReadOnly)) status_ = "error";
        else {
            readable.close();
            QSettings settings(file_, QSettings::IniFormat);
            settings.setFallbacksEnabled(false);
            const auto version = settings.value("FormatVersion", "1").toString();
            const auto theme = settings.value("Interface/Theme", "system").toString();
            const auto sidebar = settings.value("Interface/SidebarExpanded", "true").toString();
            if (settings.status() != QSettings::NoError || version != "1" || !validTheme(theme) ||
                (sidebar != "true" && sidebar != "false")) {
                blocked_ = true;
                status_ = "invalid";
            } else {
                theme_ = theme;
                expanded_ = sidebar == "true";
                status_ = "loaded";
            }
        }
    }
    if (notify) emit changed();
}
QString UiPreferences::backupJson(const QString& theme, bool sidebarExpanded) const {
    if (!validTheme(theme)) return {};
    return QString::fromUtf8(QJsonDocument(QJsonObject{
        {"format", "openkey-ui"}, {"version", 1}, {"theme", theme},
        {"sidebarExpanded", sidebarExpanded}}).toJson(QJsonDocument::Compact));
}
QVariantMap UiPreferences::inspectBackup(const QString& json) const {
    const auto backup = parseBackup(json);
    if (backup.status != "ready") return {{"ok", false}, {"status", backup.status}};
    return {{"ok", true}, {"status", "ready"}, {"theme", backup.theme},
            {"sidebarExpanded", backup.expanded}};
}
bool UiPreferences::restoreBackup(const QString& json) {
    const auto backup = parseBackup(json);
    if (backup.status != "ready") return false;
    if (status_ == "not-read") loadInternal(false);
    return save(backup.theme, backup.expanded, false);
}
bool UiPreferences::setThemeMode(const QString& theme) {
    if (!validTheme(theme)) return false;
    if (status_ == "not-read") load();
    return save(theme, expanded_);
}
bool UiPreferences::setSidebarExpanded(bool expanded) {
    if (status_ == "not-read") load();
    return save(theme_, expanded);
}
bool UiPreferences::save(const QString& theme, bool expanded, bool preserveOnFailure) {
    if (status_ == "not-read") load();
    // Keep the in-memory choice even if persistence fails, so a later explicit
    // UI save can retry the user's current choices rather than old defaults.
    if (preserveOnFailure) {
        theme_ = theme;
        expanded_ = expanded;
    }
    if (blocked_ || unsafeFile(QFileInfo(file_))) {
        blocked_ = true;
        status_ = "invalid";
        emit changed();
        return false;
    }
    if (!QDir().mkpath(QFileInfo(file_).absolutePath())) {
        status_ = "error";
        emit changed();
        return false;
    }
    QSettings settings(file_, QSettings::IniFormat);
    settings.setFallbacksEnabled(false);
    settings.setAtomicSyncRequired(true);
    settings.setValue("FormatVersion", 1);
    settings.setValue("Interface/Theme", theme);
    settings.setValue("Interface/SidebarExpanded", expanded);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        status_ = "error";
        emit changed();
        return false;
    }
    theme_ = theme;
    expanded_ = expanded;
    status_ = "saved";
    emit changed();
    return true;
}
