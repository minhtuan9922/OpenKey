#include "HotkeyOverview.h"

#include <QDBusArgument>
#include <QDBusPendingCallWatcher>
#include <QDBusServiceWatcher>
#include <QDBusVariant>
#include <QSet>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
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
    if (state_ == "loading" || state_ == "applying") return;
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
        owner_ = reply.arguments().first().toString();
        call(owner_, false, [this](const QDBusMessage& config) {
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

namespace {
bool validatedKeys(const QString& text, QStringList& keys) {
    if (text.size() > 1024) return false;
    if (text.isEmpty()) return true;
    // Deliberately bounded editor grammar; existing other Fcitx key names stay readable.
    const QRegularExpression pattern("^(?:(?:Control|Alt|Shift|Super)\\+)*(?:[a-z0-9]|F[1-9]|F[12][0-9]|F3[0-5]|space|Return|Tab|Escape|BackSpace|Delete|Insert|Home|End|Page_Up|Page_Down|Up|Down|Left|Right|Shift_L|Shift_R|Control_L|Control_R|Alt_L|Alt_R|Super_L|Super_R)$");
    keys = text.split('\n');
    if (keys.size() > 8) return false;
    QSet<QString> unique;
    for (const auto& key : keys) {
        if (!pattern.match(key).hasMatch() || unique.contains(key)) return false;
        auto parts = key.split('+');
        parts.removeLast();
        QSet<QString> mods;
        for (const auto& part : parts) { if (mods.contains(part)) return false; mods.insert(part); }
        unique.insert(key);
    }
    return true;
}
QString canonicalKey(const QString& key) {
    auto parts = key.split('+');
    const auto symbol = parts.takeLast();
    parts.sort();
    parts.append(symbol);
    return parts.join('+');
}
bool conflicts(const QVariantMap& shortcuts, const QString& field, const QStringList& keys) {
    QSet<QString> used;
    for (auto it = shortcuts.cbegin(); it != shortcuts.cend(); ++it)
        if (it.key() != field) for (const auto& key : it.value().toStringList()) used.insert(canonicalKey(key));
    for (const auto& key : keys) {
        const auto canonical = canonicalKey(key);
        if (used.contains(canonical)) return true;
        used.insert(canonical);
    }
    return false;
}
}
void HotkeyOverview::write(const QString& owner, const QVariantMap& patch, Callback callback) {
    auto request = QDBusMessage::createMethodCall(owner, "/controller", "org.fcitx.Fcitx.Controller1", "SetConfig");
    request.setAutoStartService(false);
    request.setArguments({QString("fcitx://config/global"), QVariant::fromValue(QDBusVariant(patch))});
    const auto generation = generation_;
    auto* pending = new QDBusPendingCallWatcher(bus_.asyncCall(request, 1500), this);
    connect(pending, &QDBusPendingCallWatcher::finished, this, [this, pending, generation, callback = std::move(callback)] {
        const QDBusMessage reply = pending->reply();
        pending->deleteLater();
        if (generation == generation_) callback(reply);
    });
}
void HotkeyOverview::apply(const QString& field, const QString& text) {
    if (state_ == "loading" || state_ == "applying") return;
    QStringList keys;
    if (!fields.contains(field) || !validatedKeys(text, keys)) { state_ = "invalid"; emit changed(); return; }
    if (state_ != "ready" || owner_.isEmpty()) return;
    if (conflicts(shortcuts_, field, keys)) { state_ = "duplicate"; emit changed(); return; }
    const auto expected = shortcuts_;
    const auto expectedOwner = owner_;
    ++generation_;
    state_ = "applying";
    emit changed();
    call("org.freedesktop.DBus", true, [this, field, keys, expected, expectedOwner](const QDBusMessage& owner) {
        if (owner.type() != QDBusMessage::ReplyMessage || owner.signature() != "s" ||
            owner.arguments().first().toString() != expectedOwner) { finish("unavailable"); return; }
        call(expectedOwner, false, [this, field, keys, expected, expectedOwner](const QDBusMessage& fresh) {
            QVariantMap current;
            if (!decode(fresh, current)) { finish("error"); return; }
            if (current != expected) { finish("conflict"); return; }
            if (current.value(field).toStringList() == keys) { finish("ready", current); return; }
            QVariantMap indexed;
            for (int i = 0; i < keys.size(); ++i) indexed[QString::number(i)] = keys[i];
            const QVariant value = keys.isEmpty() ? QVariant(QString()) : QVariant(indexed);
            write(expectedOwner, {{"Hotkey", QVariantMap{{field, value}}}}, [this, field, keys, expectedOwner](const QDBusMessage& ack) {
                if (ack.type() != QDBusMessage::ReplyMessage || !ack.arguments().isEmpty()) { finish("unconfirmed"); return; }
                call(expectedOwner, false, [this, field, keys](const QDBusMessage& reply) {
                    QVariantMap actual;
                    if (!decode(reply, actual) || actual.value(field).toStringList() != keys) { finish("unconfirmed"); return; }
                    finish("ready", actual);
                });
            });
        });
    });
}
QString HotkeyOverview::backupJson() const {
    if (state_ != "ready") return {};
    QJsonObject keys;
    for (auto it = shortcuts_.cbegin(); it != shortcuts_.cend(); ++it)
        keys[it.key()] = QJsonArray::fromStringList(it.value().toStringList());
    return QString::fromUtf8(QJsonDocument(QJsonObject{{"format", "openkey-hotkeys"}, {"version", 1}, {"shortcuts", keys}}).toJson());
}
QVariantMap HotkeyOverview::inspectBackup(const QString& json) const { return inspectBackupJson(json); }
QVariantMap HotkeyOverview::inspectBackupJson(const QString& json) {
    if (json.size() > 32768 || QString::fromUtf8(json.toUtf8()) != json) return {{"ok", false}};
    const auto doc = QJsonDocument::fromJson(json.toUtf8());
    const auto root = doc.object(), source = root.value("shortcuts").toObject();
    if (!doc.isObject() || root.size() != 3 || root.value("format") != "openkey-hotkeys" ||
        root.value("version") != QJsonValue(1) || source.size() != fields.size()) return {{"ok", false}};
    QVariantMap next;
    for (const auto& field : fields) {
        const auto value = source.value(field);
        if (!value.isArray()) return {{"ok", false}};
        QStringList keys;
        for (const auto& key : value.toArray()) {
            if (!key.isString()) return {{"ok", false}};
            keys.append(key.toString());
        }
        // Backup is read-only preview; original names may exceed the editor grammar.
        if (keys.size() > 32) return {{"ok", false}};
        for (const auto& key : keys) {
            if (key.isEmpty() || key.size() > 128) return {{"ok", false}};
            for (const auto ch : key) if (ch.category() == QChar::Other_Control) return {{"ok", false}};
        }
        next[field] = keys;
    }
    return {{"ok", true}, {"shortcuts", next}};
}
