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
bool validStyle(const QString& style) {
    return style == "Preedit" || style == "Direct" || style == "CommitOnly" || style == "NativeDirect";
}
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
            values_.clear();
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
    if (!ready) values_.clear();
    emit changed();
}

void SettingsController::accept(const QString& result, const QVariantMap& values) {
    values_ = values;
    finish(result, true);
}

bool SettingsController::decode(const QDBusMessage& reply, QVariantMap& values) {
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
    bool commitOnlySupported = false;
    bool nativeDirectSupported = false;
    int spellingEntries = 0, modernEntries = 0;
    bool optionalTypesValid = true;
    int quickTelexEntries = 0;
    bool quickTelexTypeValid = true;
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
                if ((values.size() < 2 || values.size() > 4) || !stringValue(values.value("0"), "Preedit") ||
                    !stringValue(values.value("1"), "Direct")) return false;
                commitOnlySupported = values.size() >= 3 && stringValue(values.value("2"), "CommitOnly");
                if (values.size() >= 3 && !commitOnlySupported) return false;
                nativeDirectSupported = values.size() == 4 && stringValue(values.value("3"), "NativeDirect");
                if (values.size() == 4 && !nativeDirectSupported) return false;
                foundStyle = true;
            } else if (name == "DirectDiagnostics") {
                if (foundDiagnostics || type != "Boolean") return false;
                foundDiagnostics = true;
            } else if (name == "QuickTelex") {
                ++quickTelexEntries;
                quickTelexTypeValid = quickTelexTypeValid && type == "Boolean";
            } else if (name == "CheckSpelling" || name == "UseModernOrthography") {
                if (name == "CheckSpelling") ++spellingEntries;
                else ++modernEntries;
                optionalTypesValid = optionalTypesValid && type == "Boolean";
            }
        }
        schema.endArray();
        schema.endStructure();
    }
    schema.endArray();
    if (!foundStyle || !foundDiagnostics) return false;
    if (value.toString() == "CommitOnly" && !commitOnlySupported) return false;
    if (value.toString() == "NativeDirect" && !nativeDirectSupported) return false;
    values = {{"InputStyle", value}, {"DirectDiagnostics", diagnostics},
              {"CommitOnlySupported", commitOnlySupported}, {"NativeDirectSupported", nativeDirectSupported}};
    const auto check = raw.value("CheckSpelling"), modern = raw.value("UseModernOrthography");
    if (spellingEntries == 1 && modernEntries == 1 && optionalTypesValid &&
        (stringValue(check, "True") || stringValue(check, "False")) &&
        (stringValue(modern, "True") || stringValue(modern, "False"))) {
        values.insert("CheckSpelling", check);
        values.insert("UseModernOrthography", modern);
    }
    const auto quickTelex = raw.value("QuickTelex");
    if (quickTelexEntries == 1 && quickTelexTypeValid &&
        (stringValue(quickTelex, "True") || stringValue(quickTelex, "False")))
        values.insert("QuickTelex", quickTelex);
    return true;
}

void SettingsController::reload() {
    if (busy_) return;
    ++generation_;
    busy_ = true;
    ready_ = false;
    writing_ = false;
    values_.clear();
    result_ = "loading";
    emit changed();
    resolve([this](const QDBusMessage& reply) {
        owner_ = replyOwner(reply);
        if (owner_.isEmpty()) { finish("unavailable"); return; }
        read([this](const QDBusMessage& config) {
            QVariantMap values;
            if (!decode(config, values)) { finish("error"); return; }
            accept("loaded", values);
        });
    });
}

void SettingsController::apply(const QString& target) {
    applyValue("InputStyle", target);
}
void SettingsController::applyDiagnostics(const QString& target) {
    applyValue("DirectDiagnostics", target);
}
void SettingsController::applySpelling(const QString& target) { applyValue("CheckSpelling", target); }
void SettingsController::applyModernOrthography(const QString& target) { applyValue("UseModernOrthography", target); }
void SettingsController::applyQuickTelex(const QString& target) { applyValue("QuickTelex", target); }
void SettingsController::applyValue(const QString& key, const QString& target) {
    if (busy_) return;
    const bool styleField = key == "InputStyle";
    const bool spellingField = key == "CheckSpelling" || key == "UseModernOrthography";
    const bool optionalField = spellingField || key == "QuickTelex";
    if (!(styleField ? validStyle(target) : (key == "DirectDiagnostics" || optionalField) && (target == "True" || target == "False"))) {
        finish("invalid"); return;
    }
    if (!ready_ || owner_.isEmpty()) { finish("unavailable"); return; }
    if (styleField && target == "CommitOnly" && !commitOnlySupported()) { finish("unsupported"); return; }
    if (styleField && target == "NativeDirect" && !nativeDirectSupported()) { finish("unsupported"); return; }
    if (spellingField && !spellingSupported()) { finish("unsupported"); return; }
    if (key == "QuickTelex" && !quickTelexSupported()) { finish("unsupported"); return; }
    const auto expected = values_.value(key).toString();
    busy_ = true;
    ready_ = false;
    result_ = "applying";
    emit changed();
    resolve([this, key, optionalField, expected, target](const QDBusMessage& reply) {
        if (replyOwner(reply) != owner_) { finish("unavailable"); return; }
        read([this, key, optionalField, expected, target](const QDBusMessage& fresh) {
            QVariantMap values;
            if (!decode(fresh, values)) { finish("error"); return; }
            if (key == "InputStyle" && target == "CommitOnly" &&
                !values.value("CommitOnlySupported").toBool()) { finish("unsupported"); return; }
            if (key == "InputStyle" && target == "NativeDirect" &&
                !values.value("NativeDirectSupported").toBool()) { finish("unsupported"); return; }
            if (optionalField && !values.contains(key)) { finish("unsupported"); return; }
            const auto current = values.value(key).toString();
            if (current != expected) { finish("conflict"); return; }
            if (current == target) { accept("confirmed", values); return; }
            // Patch one validated key. Partial load retains the other options.
            const QVariantMap patch{{key, target}};
            writing_ = true; // Set may take effect even if its reply is lost.
            call(owner_, "SetConfig", {uri, QVariant::fromValue(QDBusVariant(patch))},
                [this, key, target](const QDBusMessage& ack) {
                    if (ack.type() != QDBusMessage::ReplyMessage || !ack.arguments().isEmpty()) {
                        finish("unconfirmed"); return;
                    }
                    read([this, key, target](const QDBusMessage& confirmation) {
                        QVariantMap actual;
                        if (!decode(confirmation, actual) || actual.value(key).toString() != target) {
                            finish("unconfirmed"); return;
                        }
                        accept("confirmed", actual);
                    });
                });
        });
    });
}
