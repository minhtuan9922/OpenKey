#include "HotkeyOverview.h"

#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QSet>
#include <utility>

namespace {
const QString service = QStringLiteral("org.fcitx.Fcitx5");
const QStringList fields = {
    QStringLiteral("TriggerKeys"), QStringLiteral("AltTriggerKeys"),
    QStringLiteral("ActivateKeys"), QStringLiteral("DeactivateKeys"),
    QStringLiteral("EnumerateForwardKeys"), QStringLiteral("EnumerateBackwardKeys"),
    QStringLiteral("EnumerateGroupForwardKeys"), QStringLiteral("EnumerateGroupBackwardKeys")};
bool mapValue(const QVariant& value, QVariantMap& map) {
    if (value.metaType() == QMetaType::fromType<QVariantMap>()) {
        map = value.toMap();
        return true;
    }
    if (value.metaType() != QMetaType::fromType<QDBusArgument>()) return false;
    const auto argument = value.value<QDBusArgument>();
    if (argument.currentSignature() != "a{sv}") return false;
    map = qdbus_cast<QVariantMap>(argument);
    return true;
}
bool keyList(const QVariant& value, QStringList& keys) {
    if (value.metaType().id() == QMetaType::QString) return value.toString().isEmpty();
    QVariantMap indexed;
    if (!mapValue(value, indexed) || indexed.size() > 32) return false;
    // Numeric lookup is essential: QVariantMap's lexical order puts 10 before 2.
    for (int i = 0; i < indexed.size(); ++i) {
        const auto item = indexed.value(QString::number(i));
        if (item.metaType().id() != QMetaType::QString) return false;
        const QString key = item.toString();
        if (key.isEmpty() || key.size() > 128) return false;
        for (const QChar c : key)
            if (c.category() == QChar::Other_Control) return false;
        keys.append(key);
    }
    return true;
}
}

HotkeyOverview::HotkeyOverview(const QDBusConnection& bus, QObject* parent)
    : QObject(parent), bus_(bus) {
    auto* watcher = new QDBusServiceWatcher(service, bus_, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this,
            [this](const QString&, const QString&, const QString&) {
        ++generation_; // Logically cancel every outstanding callback; never retry automatically.
        finish(QStringLiteral("unavailable"));
    });
}

void HotkeyOverview::finish(const QString& state, const QVariantMap& shortcuts) {
    state_ = state;
    shortcuts_ = shortcuts;
    emit changed();
}

void HotkeyOverview::call(const QString& destination, bool resolve, Callback callback) {
    auto message = QDBusMessage::createMethodCall(destination,
        resolve ? "/org/freedesktop/DBus" : "/controller",
        resolve ? "org.freedesktop.DBus" : "org.fcitx.Fcitx.Controller1",
        resolve ? "GetNameOwner" : "GetConfig");
    message.setAutoStartService(false);
    message.setArguments({resolve ? service : QStringLiteral("fcitx://config/global")});
    const auto generation = generation_;
    auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(message, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this,
            [this, pending, generation, callback = std::move(callback)] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation == generation_) callback(reply);
    });
}

void HotkeyOverview::reload() {
    if (state_ == "loading") return;
    ++generation_;
    finish(QStringLiteral("loading"));
    call(QStringLiteral("org.freedesktop.DBus"), true, [this](const QDBusMessage& reply) {
        if (reply.type() == QDBusMessage::ErrorMessage) {
            finish(reply.errorName() == QStringLiteral("org.freedesktop.DBus.Error.NameHasNoOwner")
                       ? QStringLiteral("unavailable") : QStringLiteral("error"));
            return;
        }
        if (reply.signature() != "s" || reply.arguments().size() != 1 ||
            !reply.arguments().first().toString().startsWith(':')) {
            finish(QStringLiteral("error"));
            return;
        }
        // Pin the unique owner so a replacement cannot answer this generation's read.
        call(reply.arguments().first().toString(), false, [this](const QDBusMessage& config) {
            QVariantMap shortcuts;
            if (!decode(config, shortcuts)) { finish(QStringLiteral("error")); return; }
            finish(QStringLiteral("ready"), shortcuts);
        });
    });
}

bool HotkeyOverview::decode(const QDBusMessage& reply, QVariantMap& shortcuts) {
    if (reply.type() != QDBusMessage::ReplyMessage || reply.signature() != "va(sa(sssva{sv}))" ||
        reply.arguments().size() != 2 ||
        reply.arguments()[0].metaType() != QMetaType::fromType<QDBusVariant>() ||
        reply.arguments()[1].metaType() != QMetaType::fromType<QDBusArgument>()) return false;
    QVariantMap raw, hotkeys;
    if (!mapValue(reply.arguments()[0].value<QDBusVariant>().variant(), raw) ||
        !mapValue(raw.value(QStringLiteral("Hotkey")), hotkeys)) return false;
    const auto schema = reply.arguments()[1].value<QDBusArgument>();
    if (schema.currentSignature() != "a(sa(sssva{sv}))") return false;
    int types = 0, options = 0, associations = 0, globals = 0, hotkeyTypes = 0;
    QSet<QString> found;
    schema.beginArray();
    while (!schema.atEnd()) {
        if (++types > 32) return false;
        QString typeName;
        schema.beginStructure();
        schema >> typeName;
        if (typeName == "GlobalConfig" && ++globals > 1) return false;
        if (typeName == "Hotkey$HotkeyConfig" && ++hotkeyTypes > 1) return false;
        schema.beginArray();
        while (!schema.atEnd()) {
            if (++options > 128) return false;
            QString name, type, description;
            QDBusVariant defaultValue;
            QVariantMap extra;
            schema.beginStructure();
            schema >> name >> type >> description >> defaultValue >> extra;
            schema.endStructure();
            if (typeName == "GlobalConfig" && name == "Hotkey") {
                if (++associations > 1 || type != "Hotkey$HotkeyConfig") return false;
            }
            if (typeName == "Hotkey$HotkeyConfig" && fields.contains(name)) {
                if (found.contains(name) || type != "List|Key") return false;
                found.insert(name);
            }
        }
        schema.endArray();
        schema.endStructure();
    }
    schema.endArray();
    if (associations != 1 || found.size() != fields.size()) return false;
    QVariantMap decoded;
    for (const auto& name : fields) {
        QStringList keys;
        if (!keyList(hotkeys.value(name), keys)) return false;
        decoded.insert(name, keys);
    }
    shortcuts = decoded;
    return true;
}
