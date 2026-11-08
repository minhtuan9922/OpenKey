#include "ApplicationMemoryBackup.h"
#include "../ApplicationMemory.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

QString ApplicationMemoryBackup::encode(const QString& encoded) {
    openkey::ApplicationMemory checked;
    openkey::MacroTable::Entries entries;
    if (!checked.load(encoded.toStdString()) || !openkey::MacroTable::parse(encoded.toStdString(), entries, 128)) return {};
    QJsonArray rows;
    for (const auto& [app, method] : entries)
        rows.append(QJsonObject{{"application", QString::fromStdString(app)}, {"method", QString::fromStdString(method)}});
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"format", "openkey-application-memory"}, {"version", 1}, {"entries", rows}}).toJson());
}
bool ApplicationMemoryBackup::decode(const QString& json, openkey::MacroTable::Entries& entries) {
    if (json.size() > MaxText || QString::fromUtf8(json.toUtf8()) != json) return false;
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(json.toUtf8(), &error);
    const auto root = document.object();
    if (error.error != QJsonParseError::NoError || !document.isObject() || root.size() != 3 ||
        root.value("format") != "openkey-application-memory" || root.value("version") != QJsonValue(1) ||
        !root.value("entries").isArray() || root.value("entries").toArray().size() > 128) return false;
    openkey::MacroTable::Entries next;
    for (const auto& value : root.value("entries").toArray()) {
        const auto row = value.toObject();
        if (!value.isObject() || row.size() != 2 || !row.value("application").isString() || !row.value("method").isString()) return false;
        const auto app = row.value("application").toString().toStdString(), method = row.value("method").toString().toStdString();
        if (!openkey::ApplicationRules::identifiable(app) || !openkey::ApplicationMemory::supported(method) || !next.emplace(app, method).second) return false;
    }
    entries = std::move(next);
    return true;
}
QVariantMap ApplicationMemoryBackup::plan(const QString& json, const QString& mode, const QString& current) {
    const auto invalid = [](const QString& status) { return QVariantMap{{"ok", false}, {"status", status}}; };
    openkey::MacroTable::Entries imported, before;
    openkey::ApplicationMemory checked;
    if ((mode != "merge" && mode != "replace") || !decode(json, imported) || !checked.load(current.toStdString()) ||
        !openkey::MacroTable::parse(current.toStdString(), before, 128)) return invalid("invalid");
    auto after = mode == "merge" ? before : openkey::MacroTable::Entries();
    for (const auto& [app, method] : imported) after[app] = method;
    if (after.size() > openkey::ApplicationMemory::MaxApplications) return invalid("capacity");
    QVariantList changes;
    int added = 0, updated = 0, removed = 0;
    const auto change = [&](const std::string& app, const std::string& oldMethod, const std::string& newMethod) {
        changes.append(QVariantMap{{"application", QString::fromStdString(app)},
            {"before", QString::fromStdString(oldMethod)}, {"after", QString::fromStdString(newMethod)}});
    };
    for (const auto& [app, method] : after) {
        const auto found = before.find(app);
        if (found == before.end()) { ++added; change(app, "", method); }
        else if (found->second != method) { ++updated; change(app, found->second, method); }
    }
    for (const auto& [app, method] : before)
        if (!after.count(app)) { ++removed; change(app, method, ""); }
    return {{"ok", true}, {"status", "valid"}, {"added", added}, {"updated", updated}, {"removed", removed},
        {"total", static_cast<int>(after.size())}, {"changes", changes}, {"target", QString::fromStdString(openkey::MacroTable::encode(after))}};
}
