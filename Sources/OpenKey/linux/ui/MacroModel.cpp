#include "MacroModel.h"
#include "../MacroTable.h"
#include "../ApplicationRules.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

QString MacroModel::encoded() const {
    openkey::MacroTable::Entries entries;
    for (const auto& row : entries_) {
        const auto data = row.toMap();
        entries.emplace(data.value("key").toString().toUtf8().toStdString(),
                        data.value("value").toString().toUtf8().toStdString());
    }
    return QString::fromStdString(openkey::MacroTable::encode(entries));
}
bool MacroModel::fail() { status_ = "invalid"; emit changed(); return false; }
bool MacroModel::load(const QString& encoded) {
    openkey::MacroTable::Entries parsed;
    if (encoded.size() > static_cast<qsizetype>(openkey::MacroTable::MaxEncoded) ||
        !(applicationRules_ ? openkey::ApplicationRules::parse(encoded.toStdString(), parsed) : openkey::MacroTable::parse(encoded.toStdString(), parsed))) return fail();
    QVariantList next;
    for (const auto& [key, value] : parsed)
        next.append(QVariantMap{{"key", QString::fromStdString(key)}, {"value", QString::fromStdString(value)}});
    entries_ = next;
    status_ = "draft";
    emit changed();
    return true;
}
bool MacroModel::put(const QString& key, const QString& value) {
    if (key.toUtf8().size() > 128 || value.toUtf8().size() > 4096 ||
        QString::fromUtf8(key.toUtf8()) != key || QString::fromUtf8(value.toUtf8()) != value ||
        !(applicationRules_ ? openkey::ApplicationRules::validEntry(key.toUtf8().toStdString(), value.toUtf8().toStdString()) : openkey::MacroTable::validEntry(key.toUtf8().toStdString(), value.toUtf8().toStdString()))) return fail();
    openkey::MacroTable::Entries next;
    if (!(applicationRules_ ? openkey::ApplicationRules::parse(encoded().toStdString(), next) : openkey::MacroTable::parse(encoded().toStdString(), next))) return fail();
    next[key.toUtf8().toStdString()] = value.toUtf8().toStdString();
    return load(QString::fromStdString(openkey::MacroTable::encode(next)));
}
bool MacroModel::remove(const QString& key) {
    for (qsizetype i = 0; i < entries_.size(); ++i) {
        if (entries_[i].toMap().value("key").toString() == key) {
            entries_.removeAt(i); status_ = "draft"; emit changed(); return true;
        }
    }
    return false;
}
QString MacroModel::exportJson() const {
    QJsonArray rows;
    for (const auto& row : entries_) rows.append(QJsonObject::fromVariantMap(row.toMap()));
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"format", applicationRules_ ? "openkey-app-rules" : "openkey-macros"}, {"version", 1}, {"entries", rows}})
                             .toJson(QJsonDocument::Indented));
}
bool MacroModel::importJson(const QString& json) {
    if (json.size() > 131072 || QString::fromUtf8(json.toUtf8()) != json) return fail();
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(json.toUtf8(), &error);
    const auto root = doc.object();
    if (error.error != QJsonParseError::NoError || !doc.isObject() || root.size() != 3 ||
        root.value("format") != (applicationRules_ ? "openkey-app-rules" : "openkey-macros") || root.value("version") != QJsonValue(1) ||
        !root.value("entries").isArray() || root.value("entries").toArray().size() > 256) return fail();
    openkey::MacroTable::Entries next;
    for (const auto& row : root.value("entries").toArray()) {
        const auto obj = row.toObject();
        if (!row.isObject() || obj.size() != 2 || !obj.value("key").isString() || !obj.value("value").isString()) return fail();
        const auto key = obj.value("key").toString().toUtf8().toStdString();
        const auto value = obj.value("value").toString().toUtf8().toStdString();
        if (!(applicationRules_ ? openkey::ApplicationRules::validEntry(key, value) : openkey::MacroTable::validEntry(key, value)) || !next.emplace(key, value).second) return fail();
    }
    return load(QString::fromStdString(openkey::MacroTable::encode(next)));
}
