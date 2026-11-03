#include "SettingsController.h"
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QVariantMap>
#include <utility>

namespace {
const QString service = QStringLiteral("org.fcitx.Fcitx5");
const QString uri = QStringLiteral("fcitx://config/addon/openkey");
bool validStyle(const QString& style) { return style == "Preedit" || style == "Direct"; }
QString replyOwner(const QDBusMessage& reply) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.signature() != "s" || reply.arguments().size() != 1)
        return {};
    const auto owner = reply.arguments().first().toString();
    return owner.startsWith(':') ? owner : QString();
}
QVariantMap variantMap(const QVariant& value) {
    if (value.metaType() == QMetaType::fromType<QVariantMap>()) return value.toMap();
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) return {};
    const auto arg = value.value<QDBusArgument>();
    return arg.currentSignature() == "a{sv}" ? qdbus_cast<QVariantMap>(arg) : QVariantMap();
}
bool stringValue(const QVariant& value, const QString& expected) {
    return value.metaType().id() == QMetaType::QString && value.toString() == expected;
}
}

SettingsController::SettingsController(const QDBusConnection& bus, QObject* parent)
    : QObject(parent), bus_(bus) {
    auto* watcher = new QDBusServiceWatcher(service, bus_, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
        [this](const QString&, const QString&, const QString&) {
            ++generation_; // Outstanding replies from the old owner must not re-enable Apply.
            const bool uncertain = writing_;
            owner_.clear();
            style_.clear();
            finish(uncertain ? "unconfirmed" : "unavailable");
        });
}

void SettingsController::call(const QString& destination, const QString& method,
                              const QVariantList& arguments, Callback callback, bool busMethod) {
    auto message = QDBusMessage::createMethodCall(destination,
        busMethod ? "/org/freedesktop/DBus" : "/controller",
        busMethod ? "org.freedesktop.DBus" : "org.fcitx.Fcitx.Controller1", method);
    message.setAutoStartService(false);
    message.setArguments(arguments);
    const auto generation = generation_;
    auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(message, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
        [this, pending, generation, callback = std::move(callback)] {
            const QDBusMessage reply = pending->reply();
            pending->deleteLater();
            if (generation == generation_) callback(reply);
        });
}
void SettingsController::resolve(Callback callback) {
    call("org.freedesktop.DBus", "GetNameOwner", {service}, std::move(callback), true);
}
void SettingsController::read(Callback callback) { call(owner_, "GetConfig", {uri}, std::move(callback)); }
void SettingsController::finish(const QString& result, bool ready) {
    busy_ = false;
    writing_ = false;
    ready_ = ready;
    result_ = result;
    if (!ready) style_.clear();
    emit changed();
}

bool SettingsController::decode(const QDBusMessage& reply, QString& style) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.signature() != "va(sa(sssva{sv}))" ||
        reply.arguments().size() != 2) return false;
    const auto raw = variantMap(reply.arguments()[0].value<QDBusVariant>().variant());
    const auto value = raw.value("InputStyle");
    const auto diagnostics = raw.value("DirectDiagnostics");
    if (value.metaType().id() != QMetaType::QString || !validStyle(value.toString()) ||
        (!stringValue(diagnostics, "True") && !stringValue(diagnostics, "False"))) return false;
    const auto schema = reply.arguments()[1].value<QDBusArgument>();
    if (schema.currentSignature() != "a(sa(sssva{sv}))") return false;
    bool foundStyle = false, foundDiagnostics = false;
    int types = 0, options = 0;
    schema.beginArray();
    while (!schema.atEnd()) {
        if (++types > 32) return false;
        QString typeName;
        schema.beginStructure();
        schema >> typeName;
        schema.beginArray();
        while (!schema.atEnd()) {
            if (++options > 128) return false;
            QString name, type, description;
            QDBusVariant defaultValue;
            QVariantMap extra;
            schema.beginStructure();
            schema >> name >> type >> description >> defaultValue >> extra;
            schema.endStructure();
            if (typeName != "InputConfig") continue;
            if (name == "InputStyle") {
                if (foundStyle || type != "Enum") return false;
                const auto values = variantMap(extra.value("Enum"));
                if (values.size() != 2 || !stringValue(values.value("0"), "Preedit") ||
                    !stringValue(values.value("1"), "Direct")) return false;
                foundStyle = true;
            } else if (name == "DirectDiagnostics") {
                if (foundDiagnostics || type != "Boolean") return false;
                foundDiagnostics = true;
            }
        }
        schema.endArray();
        schema.endStructure();
    }
    schema.endArray();
    if (!foundStyle || !foundDiagnostics) return false;
    style = value.toString();
    return true;
}

void SettingsController::reload() {
    if (busy_) return;
    ++generation_;
    busy_ = true;
    ready_ = false;
    writing_ = false;
    style_.clear();
    result_ = "loading";
    emit changed();
    resolve([this](const QDBusMessage& reply) {
        owner_ = replyOwner(reply);
        if (owner_.isEmpty()) { finish("unavailable"); return; }
        read([this](const QDBusMessage& config) {
            QString style;
            if (!decode(config, style)) { finish("error"); return; }
            style_ = style;
            finish("loaded", true);
        });
    });
}

void SettingsController::apply(const QString& target) {
    if (busy_) return;
    if (!validStyle(target)) { finish("invalid"); return; }
    if (!ready_ || owner_.isEmpty()) { finish("unavailable"); return; }
    const auto expected = style_;
    busy_ = true;
    ready_ = false;
    result_ = "applying";
    emit changed();
    resolve([this, expected, target](const QDBusMessage& reply) {
        if (replyOwner(reply) != owner_) { finish("unavailable"); return; }
        read([this, expected, target](const QDBusMessage& fresh) {
            QString current;
            if (!decode(fresh, current)) { finish("error"); return; }
            if (current != expected) { finish("conflict"); return; }
            if (current == target) { style_ = current; finish("confirmed", true); return; }
            // Patch only InputStyle. config_.load(raw, true) retains other options.
            const QVariantMap patch{{"InputStyle", target}};
            writing_ = true; // Set may take effect even if its reply is lost.
            call(owner_, "SetConfig", {uri, QVariant::fromValue(QDBusVariant(patch))},
                [this, target](const QDBusMessage& ack) {
                    if (ack.type() != QDBusMessage::ReplyMessage || !ack.arguments().isEmpty()) {
                        finish("unconfirmed"); return;
                    }
                    read([this, target](const QDBusMessage& confirmation) {
                        QString actual;
                        if (!decode(confirmation, actual) || actual != target) {
                            finish("unconfirmed"); return;
                        }
                        style_ = actual;
                        finish("confirmed", true);
                    });
                });
        });
    });
}
