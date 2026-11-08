#include "InputMethodController.h"
#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QSet>
#include <QJsonDocument>
#include <QJsonObject>
namespace {
const QString service = "org.fcitx.Fcitx5";
const QStringList allowed{"openkey-telex", "openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2", "keyboard-us"};
QString textReply(const QDBusMessage& reply) {
    return reply.type() == QDBusMessage::ReplyMessage && reply.signature() == "s" && reply.arguments().size() == 1 ?
        reply.arguments().first().toString() : QString();
}
}
InputMethodController::InputMethodController(const QDBusConnection& bus, QObject* parent) : QObject(parent), bus_(bus) {
    auto* watcher = new QDBusServiceWatcher(service, bus_, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString&, const QString&, const QString&) {
        ++generation_;
        const bool uncertain = writing_;
        owner_.clear();
        finish(uncertain ? "unconfirmed" : "unavailable");
    });
}
void InputMethodController::call(const QString& destination, const QString& method, const QVariantList& args, Callback callback, bool resolving) {
    auto message = QDBusMessage::createMethodCall(destination, resolving ? "/org/freedesktop/DBus" : "/controller",
        resolving ? "org.freedesktop.DBus" : "org.fcitx.Fcitx.Controller1", method);
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
void InputMethodController::resolve(Callback callback) { call("org.freedesktop.DBus", "GetNameOwner", {service}, std::move(callback), true); }
void InputMethodController::finish(const QString& result, bool ready) {
    result_ = result; ready_ = ready; busy_ = false; writing_ = false;
    if (!ready) snapshot_ = {};
    emit changed();
}
void InputMethodController::accept(const QString& result, const Snapshot& snapshot) { snapshot_ = snapshot; finish(result, true); }
void InputMethodController::read(std::function<void(const Snapshot&)> callback) {
    call(owner_, "CurrentInputMethodGroup", {}, [this, callback = std::move(callback)](const QDBusMessage& groupReply) {
        const auto group = textReply(groupReply);
        if (group.isEmpty() || group.size() > 256) { finish(writing_ ? "unconfirmed" : "error"); return; }
        call(owner_, "InputMethodGroupInfo", {group}, [this, group, callback](const QDBusMessage& info) {
            if (info.type() != QDBusMessage::ReplyMessage || info.signature() != "sa(ss)" || info.arguments().size() != 2 ||
                info.arguments()[1].metaType() != QMetaType::fromType<QDBusArgument>()) { finish(writing_ ? "unconfirmed" : "error"); return; }
            const auto array = info.arguments()[1].value<QDBusArgument>();
            if (array.currentSignature() != "a(ss)") { finish(writing_ ? "unconfirmed" : "error"); return; }
            QStringList methods;
            QSet<QString> seen;
            int count = 0;
            array.beginArray();
            while (!array.atEnd()) {
                QString name, layout;
                array.beginStructure(); array >> name >> layout; array.endStructure();
                if (++count > 256 || name.isEmpty() || name.size() > 256 || seen.contains(name)) { finish(writing_ ? "unconfirmed" : "error"); return; }
                seen.insert(name);
                if (allowed.contains(name)) methods.append(name);
            }
            array.endArray();
            call(owner_, "CurrentInputMethod", {}, [this, group, methods, callback](const QDBusMessage& methodReply) {
                const auto current = textReply(methodReply);
                if (current.isEmpty() || current.size() > 256) { finish(writing_ ? "unconfirmed" : "error"); return; }
                call(owner_, "State", {}, [this, group, methods, current, callback](const QDBusMessage& stateReply) {
                    if (stateReply.type() != QDBusMessage::ReplyMessage || stateReply.signature() != "i" || stateReply.arguments().size() != 1) { finish(writing_ ? "unconfirmed" : "error"); return; }
                    const int state = stateReply.arguments().first().toInt();
                    if (state < 0 || state > 2) { finish(writing_ ? "unconfirmed" : "error"); return; }
                    call(owner_, "CurrentInputMethodGroup", {}, [this, group, methods, current, state, callback](const QDBusMessage& check) {
                        if (textReply(check) != group) { finish(writing_ ? "unconfirmed" : "conflict"); return; }
                        callback({group, current, methods, state});
                    });
                });
            });
        });
    });
}
void InputMethodController::reload() {
    if (busy_) return;
    ++generation_;
    busy_ = true; ready_ = false; result_ = "loading"; emit changed();
    resolve([this](const QDBusMessage& reply) {
        const auto owner = textReply(reply);
        if (!owner.startsWith(':')) { finish("unavailable"); return; }
        owner_ = owner;
        read([this](const Snapshot& snapshot) { accept("loaded", snapshot); });
    });
}
void InputMethodController::select(const QString& method) {
    if (busy_) return;
    if (!allowed.contains(method)) { finish("invalid"); return; }
    if (!ready_ || owner_.isEmpty()) { finish("unavailable"); return; }
    if (!snapshot_.methods.contains(method)) { finish("not-configured"); return; }
    const auto expected = snapshot_;
    ++generation_;
    busy_ = true; ready_ = false; result_ = "applying"; emit changed();
    resolve([this, method, expected](const QDBusMessage& reply) {
        if (textReply(reply) != owner_) { finish("unavailable"); return; }
        read([this, method, expected](const Snapshot& fresh) {
            if (fresh.group != expected.group || fresh.current != expected.current || fresh.state != expected.state) { finish("conflict"); return; }
            if (!fresh.methods.contains(method)) { finish("not-configured"); return; }
            if (fresh.current == method && (method == "keyboard-us" || fresh.state == 2)) { accept("confirmed", fresh); return; }
            writing_ = true;
            call(owner_, "SetCurrentIM", {method}, [this, method](const QDBusMessage& ack) {
                if (ack.type() != QDBusMessage::ReplyMessage || !ack.arguments().isEmpty()) { finish("unconfirmed"); return; }
                read([this, method](const Snapshot& actual) {
                    if (actual.current != method || (method != "keyboard-us" && actual.state != 2)) { finish("unconfirmed"); return; }
                    accept("confirmed", actual);
                });
            });
        });
    });
}

QString InputMethodController::backupJson() const {
    if (!ready_ || !allowed.contains(snapshot_.current)) return {};
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"format", "openkey-input-method"}, {"version", 1}, {"method", snapshot_.current}}).toJson());
}
QString InputMethodController::methodFromBackup(const QString& json) {
    if (json.size() > 4096 || QString::fromUtf8(json.toUtf8()) != json) return {};
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    const auto root = doc.object();
    if (!doc.isObject() || root.size() != 3 || root.value("format") != "openkey-input-method" || root.value("version") != QJsonValue(1) ||
        !root.value("method").isString() || !allowed.contains(root.value("method").toString())) return {};
    return root.value("method").toString();
}
QVariantMap InputMethodController::inspectBackup(const QString& json) const {
    const auto method = methodFromBackup(json);
    return {{"ok", !method.isEmpty()}, {"method", method}};
}
