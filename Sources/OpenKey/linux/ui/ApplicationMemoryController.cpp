#include "ApplicationMemoryController.h"
#include "ApplicationMemoryBackup.h"
#include "../ApplicationMemory.h"
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QSet>

namespace {
const QString service = QStringLiteral("org.fcitx.Fcitx5");
const QString uri = QStringLiteral("fcitx://config/addon/openkey/application-memory");
QVariantMap mapValue(const QVariant& value) {
    if (value.metaType().id() == QMetaType::QVariantMap) return value.toMap();
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) return {};
    const auto arg = value.value<QDBusArgument>();
    return arg.currentSignature() == "a{sv}" ? qdbus_cast<QVariantMap>(arg) : QVariantMap();
}
QString ownerValue(const QDBusMessage& reply) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.signature() != "s" || reply.arguments().size() != 1) return {};
    const auto owner = reply.arguments().first().toString();
    return owner.startsWith(':') ? owner : QString();
}
}
ApplicationMemoryController::ApplicationMemoryController(const QDBusConnection& bus, QObject* parent)
    : QObject(parent), bus_(bus) {
    auto* watcher = new QDBusServiceWatcher(service, bus_, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this] {
        ++generation_;
        owner_.clear();
        finish(writing_ ? "unconfirmed" : "unavailable");
    });
}
void ApplicationMemoryController::call(const QString& destination, const QString& method,
    const QVariantList& args, Callback callback, bool resolve) {
    auto message = QDBusMessage::createMethodCall(destination, resolve ? "/org/freedesktop/DBus" : "/controller",
        resolve ? "org.freedesktop.DBus" : "org.fcitx.Fcitx.Controller1", method);
    message.setAutoStartService(false);
    message.setArguments(args);
    const auto generation = generation_;
    auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(message, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, pending, generation, callback = std::move(callback)] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation == generation_) callback(reply);
    });
}
void ApplicationMemoryController::read(Callback callback) { call(owner_, "GetConfig", {uri}, std::move(callback)); }
void ApplicationMemoryController::finish(const QString& result, bool ready) {
    cancelRefresh();
    clearPreview();
    ready_ = ready; busy_ = false; writing_ = false; result_ = result;
    if (!ready) {
        const bool hadEntries = !encoded_.isEmpty();
        encoded_.clear(); currentApplication_.clear(); identityStatus_ = "unavailable";
        if (hadEntries) emit entriesChanged();
    }
    emit changed();
}
bool ApplicationMemoryController::accept(const QDBusMessage& reply) {
    if (reply.type() != QDBusMessage::ReplyMessage ||
        reply.signature() != "va(sa(sssva{sv}))" || reply.arguments().size() != 2) return false;
    const auto raw = mapValue(reply.arguments()[0].value<QDBusVariant>().variant());
    const QSet<QString> keys{"Entries", "ExpectedEntries", "CurrentApplication", "IdentityStatus"};
    for (const auto& key : keys) if (raw.value(key).metaType().id() != QMetaType::QString) return false;
    const auto encoded = raw.value("Entries").toString();
    openkey::ApplicationMemory memory;
    if (encoded.size() > static_cast<qsizetype>(openkey::MacroTable::MaxEncoded) || !memory.load(encoded.toStdString())) return false;
    const auto app = raw.value("CurrentApplication").toString(), status = raw.value("IdentityStatus").toString();
    if ((!app.isEmpty() && !openkey::ApplicationRules::identifiable(app.toStdString())) ||
        !QStringList{"disabled", "not-observed", "waiting", "connected", "unavailable"}.contains(status)) return false;
    // Require the matching subconfig schema; a main config or a legacy addon
    // must never unlock management controls.
    const auto schema = reply.arguments()[1].value<QDBusArgument>();
    if (schema.currentSignature() != "a(sa(sssva{sv}))") return false;
    QSet<QString> found;
    int types = 0, options = 0;
    schema.beginArray();
    while (!schema.atEnd()) {
        if (++types > 32) return false;
        QString name;
        schema.beginStructure(); schema >> name; schema.beginArray();
        while (!schema.atEnd()) {
            if (++options > 128) return false;
            QString key, type, description; QDBusVariant defaultValue; QVariantMap extra;
            schema.beginStructure(); schema >> key >> type >> description >> defaultValue >> extra; schema.endStructure();
            if (name == "ApplicationMemoryView" && keys.contains(key)) {
                if (type != "String" || found.contains(key)) return false;
                found.insert(key);
            }
        }
        schema.endArray(); schema.endStructure();
    }
    schema.endArray();
    if (found != keys) return false;
    const bool tableChanged = encoded_ != encoded;
    encoded_ = encoded; currentApplication_ = app; identityStatus_ = status;
    if (tableChanged) emit entriesChanged();
    return true;
}
QVariantList ApplicationMemoryController::entries() const {
    openkey::MacroTable::Entries table;
    openkey::MacroTable::parse(encoded_.toStdString(), table, 128);
    QVariantList rows;
    for (const auto& [app, method] : table)
        rows.append(QVariantMap{{"application", QString::fromStdString(app)}, {"method", QString::fromStdString(method)}});
    return rows;
}
void ApplicationMemoryController::reload() {
    if (busy_) return;
    cancelRefresh();
    clearPreview();
    ++generation_; busy_ = true; result_ = "loading";
    emit changed();
    call("org.freedesktop.DBus", "GetNameOwner", {service}, [this](const QDBusMessage& reply) {
        owner_ = ownerValue(reply);
        if (owner_.isEmpty()) { finish("unavailable"); return; }
        read([this](const QDBusMessage& config) {
            if (!accept(config)) { finish("unsupported"); return; }
            finish("loaded", true);
        });
    }, true);
}
void ApplicationMemoryController::cancelRefresh() {
    if (!refreshing_) return;
    refreshing_ = false;
    ++generation_; // A user action supersedes the pending background snapshot.
}
void ApplicationMemoryController::refresh() {
    if (busy_ || refreshing_ || !ready_ || backupPreviewReady() || result_ != "loaded") return;
    refreshing_ = true;
    call("org.freedesktop.DBus", "GetNameOwner", {service}, [this](const QDBusMessage& reply) {
        if (ownerValue(reply) != owner_) { finish("unavailable"); return; }
        read([this](const QDBusMessage& reply) {
            const auto previousEntries = encoded_, previousApp = currentApplication_, previousStatus = identityStatus_;
            if (!accept(reply)) { finish("unsupported"); return; }
            refreshing_ = false;
            // No loading/busy transition, or signal for an unchanged sample.
            if (encoded_ != previousEntries || currentApplication_ != previousApp || identityStatus_ != previousStatus)
                emit changed();
        });
    }, true);
}
void ApplicationMemoryController::update(const QString& app, const QString& expected, const QString& method) {
    if (!openkey::ApplicationMemory::supported(method.toStdString())) { if (!busy_) finish("invalid", ready_); return; }
    edit(app, expected, method);
}
void ApplicationMemoryController::remove(const QString& app, const QString& expected) { edit(app, expected, {}); }
void ApplicationMemoryController::edit(const QString& app, const QString& expected, const QString& method) {
    if (busy_) return;
    if (!ready_ || owner_.isEmpty()) { finish("unavailable"); return; }
    openkey::ApplicationMemory memory;
    if (!memory.load(encoded_.toStdString()) || !openkey::ApplicationRules::identifiable(app.toStdString()) || expected.isEmpty()) {
        finish("invalid", true); return;
    }
    if (memory.lookup(app.toStdString()) != expected.toStdString()) { finish("conflict", true); return; }
    if (!method.isEmpty() && method == expected) { finish("confirmed", true); return; }
    const auto baseline = encoded_;
    memory.remember(app.toStdString(), method.toStdString());
    const auto target = QString::fromStdString(memory.encode());
    writeTarget(baseline, target);
}
void ApplicationMemoryController::writeTarget(const QString& baseline, const QString& target) {
    cancelRefresh();
    clearPreview();
    busy_ = true; writing_ = false; result_ = "applying"; emit changed();
    call("org.freedesktop.DBus", "GetNameOwner", {service}, [this, baseline, target](const QDBusMessage& reply) {
        if (ownerValue(reply) != owner_) { finish("unavailable"); return; }
        read([this, baseline, target](const QDBusMessage& reply) {
            if (!accept(reply)) { finish("error"); return; }
            if (encoded_ != baseline) { finish("conflict", true); return; }
            if (target == baseline) { finish("confirmed", true); return; }
            const QVariantMap patch{{"Entries", target}, {"ExpectedEntries", baseline}};
            writing_ = true;
            call(owner_, "SetConfig", {uri, QVariant::fromValue(QDBusVariant(patch))}, [this, target](const QDBusMessage& ack) {
                if (ack.type() != QDBusMessage::ReplyMessage || !ack.arguments().isEmpty()) { finish("unconfirmed"); return; }
                read([this, target](const QDBusMessage& actual) {
                    if (!accept(actual)) { finish("unconfirmed"); return; }
                    finish(encoded_ == target ? "confirmed" : "unconfirmed", true);
                });
            });
        });
    }, true);
}

void ApplicationMemoryController::clearPreview() {
    inspectedJson_.clear(); inspectedMode_.clear(); inspectedBaseline_.clear(); inspectedTarget_.clear();
}
void ApplicationMemoryController::cancelBackupPreview() {
    if (!backupPreviewReady()) return;
    clearPreview(); emit changed();
}
QString ApplicationMemoryController::backupJson() const {
    return ready_ && !busy_ ? ApplicationMemoryBackup::encode(encoded_) : QString();
}
QVariantMap ApplicationMemoryController::inspectBackup(const QString& json, const QString& mode) {
    cancelRefresh();
    clearPreview();
    if (!ready_ || busy_) { emit changed(); return {{"ok", false}, {"status", "unavailable"}}; }
    auto preview = ApplicationMemoryBackup::plan(json, mode, encoded_);
    if (preview.value("ok").toBool()) {
        inspectedJson_ = json; inspectedMode_ = mode; inspectedBaseline_ = encoded_;
        inspectedTarget_ = preview.take("target").toString();
    }
    emit changed();
    return preview;
}
void ApplicationMemoryController::restoreBackup(const QString& json, const QString& mode) {
    if (busy_) return;
    if (!ready_ || owner_.isEmpty()) { finish("unavailable"); return; }
    if (!backupPreviewReady() || json != inspectedJson_ || mode != inspectedMode_ || encoded_ != inspectedBaseline_) {
        finish("invalid", true); return;
    }
    const auto baseline = inspectedBaseline_, target = inspectedTarget_;
    writeTarget(baseline, target); // Consumes the preview before any async call.
}
