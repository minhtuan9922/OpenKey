// Always re-exec on a private bus before creating any D-Bus connection.
#include "HotkeyOverview.h"
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusError>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QElapsedTimer>
#include <QtEndian>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

namespace HotkeyWire {
struct Option {
    QString name, type, description;
    QDBusVariant defaultValue;
    QVariantMap extra;
};
using Options = QList<Option>;
struct Type {
    QString name;
    Options options;
};
using Types = QList<Type>;

QDBusArgument& operator<<(QDBusArgument& arg, const Option& option) {
    arg.beginStructure();
    arg << option.name << option.type << option.description
        << option.defaultValue << option.extra;
    arg.endStructure();
    return arg;
}
const QDBusArgument& operator>>(const QDBusArgument& arg, Option& option) {
    arg.beginStructure();
    arg >> option.name >> option.type >> option.description
        >> option.defaultValue >> option.extra;
    arg.endStructure();
    return arg;
}
QDBusArgument& operator<<(QDBusArgument& arg, const Type& type) {
    arg.beginStructure();
    arg << type.name << type.options;
    arg.endStructure();
    return arg;
}
const QDBusArgument& operator>>(const QDBusArgument& arg, Type& type) {
    arg.beginStructure();
    arg >> type.name >> type.options;
    arg.endStructure();
    return arg;
}
}
Q_DECLARE_METATYPE(HotkeyWire::Option)
Q_DECLARE_METATYPE(HotkeyWire::Options)
Q_DECLARE_METATYPE(HotkeyWire::Type)
Q_DECLARE_METATYPE(HotkeyWire::Types)

namespace {
const QString service = QStringLiteral("org.fcitx.Fcitx5");
const QString path = QStringLiteral("/controller");
const QString serverName = QStringLiteral("hotkey-test-server");
const QString clientName = QStringLiteral("hotkey-test-client");
const QStringList fields = {"TriggerKeys", "AltTriggerKeys", "ActivateKeys", "DeactivateKeys",
    "EnumerateForwardKeys", "EnumerateBackwardKeys", "EnumerateGroupForwardKeys", "EnumerateGroupBackwardKeys"};

HotkeyWire::Option option(const QString& name, const QString& type) {
    // Defaults must never be substituted for missing runtime values.
    return {name, type, name, QDBusVariant(QStringLiteral("DefaultMustNotAppear")), {}};
}
QVariantMap indexed(int count) {
    QVariantMap result;
    for (int i = 0; i < count; ++i) result.insert(QString::number(i), QStringLiteral("Key%1").arg(i));
    return result;
}
class ControllerMock : public QDBusVirtualObject {
public:
    explicit ControllerMock(const QDBusConnection& bus) : bus(bus) {
        HotkeyWire::Options options;
        for (const auto& field : fields) {
            options.append(option(field, "List|Key"));
            hotkeys.insert(field, QString());
        }
        hotkeys["TriggerKeys"] = QVariantMap{{"0", "Control+space"}, {"1", "Zenkaku_Hankaku"}, {"2", "Hangul"}};
        hotkeys["AltTriggerKeys"] = QVariantMap{{"0", "Shift_L"}};
        hotkeys["ActivateKeys"] = QVariantMap{{"0", "Hangul_Hanja"}};
        hotkeys["DeactivateKeys"] = QVariantMap{{"0", "Hangul_Romaja"}};
        hotkeys["EnumerateGroupForwardKeys"] = QVariantMap{{"0", "Super+space"}};
        hotkeys["EnumerateGroupBackwardKeys"] = QVariantMap{{"0", "Shift+Super+space"}};
        types = {{"GlobalConfig", {option("Hotkey", "Hotkey$HotkeyConfig")}}, {"Hotkey$HotkeyConfig", options}};
    }
    QDBusConnection bus;
    QVariantMap hotkeys;
    HotkeyWire::Types types;
    QVariant rawOverride;
    bool hold = false, fail = false, wrongSignature = false;
    int reads = 0, writes = 0, expectedWrites = 0;
    bool allowWrite = false, ignoreWrite = false, rejectWrite = false;
    QVariantMap lastPatch;
    QStringList violations;
    QList<QDBusMessage> pending;
    QString introspect(const QString&) const override { return {}; }
    void reply(const QDBusMessage& message) {
        if (fail) { bus.send(message.createErrorReply(QDBusError::Failed, "fixture failure")); return; }
        if (wrongSignature) { bus.send(message.createReply(QVariantList{QStringLiteral("wrong")})); return; }
        const QVariant raw = rawOverride.isValid() ? rawOverride : QVariant(QVariantMap{{"Hotkey", hotkeys}, {"Other", 42}});
        bus.send(message.createReply({QVariant::fromValue(QDBusVariant(raw)), QVariant::fromValue(types)}));
    }
    void release() {
        hold = false;
        const auto messages = pending;
        pending.clear();
        for (const auto& message : messages) reply(message);
    }
    bool handleMessage(const QDBusMessage& message, const QDBusConnection&) override {
        if (message.member() == "SetConfig" && allowWrite) {
            ++writes;
            if (message.signature() != "sv" || message.arguments().first().toString() != "fcitx://config/global")
                violations.append("Invalid write URI/signature");
            const auto decodedMap = [](const QVariant& value) {
                return value.metaType() == QMetaType::fromType<QDBusArgument>() ? qdbus_cast<QVariantMap>(value.value<QDBusArgument>()) : value.toMap();
            };
            lastPatch = decodedMap(message.arguments()[1].value<QDBusVariant>().variant());
            const auto patch = decodedMap(lastPatch.value("Hotkey"));
            if (lastPatch.size() != 1 || patch.size() != 1 || !fields.contains(patch.firstKey())) violations.append("Invalid hotkey patch");
            if (rejectWrite) bus.send(message.createErrorReply(QDBusError::Failed, "write failed"));
            else {
                if (!ignoreWrite) hotkeys[patch.firstKey()] = patch.first();
                bus.send(message.createReply());
            }
            return true;
        }
        if (message.member() != "GetConfig") {
            ++writes;
            violations.append("Unexpected method " + message.member());
            bus.send(message.createErrorReply(QDBusError::UnknownMethod, "read only"));
            return true;
        }
        ++reads;
        if (message.path() != path || message.interface() != "org.fcitx.Fcitx.Controller1" ||
            message.signature() != "s" || message.arguments().size() != 1 ||
            message.arguments().first().toString() != "fcitx://config/global") violations.append("Wrong read target");
        if (hold) pending.append(message);
        else reply(message);
        return true;
    }
};
}

class LinuxHotkeyOverviewTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QDBusConnection> server, client;
    std::unique_ptr<ControllerMock> mock;
    QProcess monitor;
    void ready(HotkeyOverview& model) {
        model.reload();
        QTRY_COMPARE(model.state(), QStringLiteral("ready"));
        QCOMPARE(model.shortcuts().size(), 8);
        for (const auto& value : model.shortcuts()) QCOMPARE(value.metaType(), QMetaType::fromType<QStringList>());
    }
private slots:
    void initTestCase() {
        QVERIFY(qEnvironmentVariableIsSet("OPENKEY_HOTKEY_PRIVATE_BUS"));
        qDBusRegisterMetaType<HotkeyWire::Option>();
        qDBusRegisterMetaType<HotkeyWire::Options>();
        qDBusRegisterMetaType<HotkeyWire::Type>();
        qDBusRegisterMetaType<HotkeyWire::Types>();
    }
    void init() {
        const auto address = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        server = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, serverName));
        client = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, clientName));
        QVERIFY(server->isConnected());
        QVERIFY(client->isConnected());
        QVERIFY(server->baseService() != client->baseService());
        mock = std::make_unique<ControllerMock>(*server);
        QVERIFY(server->registerVirtualObject(path, mock.get()));
        QVERIFY(server->registerService(service));
        // Qt does not preserve incoming autoStartService() flags. Inspect the actual
        // wire headers on this private bus instead of asserting Qt's default value.
        monitor.start(QStringLiteral("dbus-monitor"), {QStringLiteral("--binary"),
            QStringLiteral("type='method_call',interface='org.fcitx.Fcitx.Controller1'")});
        QVERIFY(monitor.waitForStarted());
        QVERIFY(monitor.waitForReadyRead(1000));
    }
    void cleanup() {
        monitor.waitForReadyRead(30);
        const QByteArray wire = monitor.readAllStandardOutput();
        monitor.terminate();
        if (!monitor.waitForFinished(1000)) { monitor.kill(); monitor.waitForFinished(); }
        int calls = 0;
        qsizetype offset = 0;
        while (offset + 16 <= wire.size()) {
            const auto* header = reinterpret_cast<const uchar*>(wire.constData() + offset);
            QVERIFY(header[0] == 'l' || header[0] == 'B');
            const auto read32 = [header](int index) {
                return header[0] == 'l' ? qFromLittleEndian<quint32>(header + index)
                                        : qFromBigEndian<quint32>(header + index);
            };
            const quint64 size = ((quint64(16) + read32(12) + 7) & ~quint64(7)) + read32(4);
            QVERIFY(size <= quint64(wire.size() - offset));
            if (header[1] == 1) {
                ++calls;
                QVERIFY2(header[2] & 0x02, "D-Bus NO_AUTO_START flag missing");
            }
            offset += qsizetype(size);
        }
        QCOMPARE(offset, wire.size());
        QVERIFY(calls >= mock->reads);
        QCOMPARE(mock->writes, mock->expectedWrites);
        QVERIFY2(mock->violations.isEmpty(), qPrintable(mock->violations.join('\n')));
        server->unregisterService(service);
        server->unregisterObject(path);
        mock.reset();
        server.reset();
        client.reset();
        QDBusConnection::disconnectFromBus(serverName);
        QDBusConnection::disconnectFromBus(clientName);
    }
    void editorAppliesOneField() {
        HotkeyOverview model(*client);
        ready(model);
        const auto original = model.shortcuts();
        const auto json = model.backupJson();
        const auto inspected = model.inspectBackup(json);
        QVERIFY(inspected.value("ok").toBool());
        QCOMPARE(inspected.value("shortcuts").toMap(), original);
        QVERIFY(!model.inspectBackup("{}").value("ok").toBool());
        mock->allowWrite = true;
        mock->expectedWrites = 1;
        model.apply("TriggerKeys", "Control+Alt+v\nF6");
        QTRY_COMPARE(model.state(), QString("ready"));
        QCOMPARE(mock->writes, 1);
        QCOMPARE(model.shortcuts().value("TriggerKeys").toStringList(), (QStringList{"Control+Alt+v", "F6"}));
        QCOMPARE(model.shortcuts().value("AltTriggerKeys"), original.value("AltTriggerKeys"));
        model.apply("TriggerKeys", "Control+Alt+v\nF6");
        QTRY_COMPARE(model.state(), QString("ready"));
        QCOMPARE(mock->writes, 1); // No-op never writes.
        mock->expectedWrites = 2;
        model.apply("TriggerKeys", "");
        QTRY_COMPARE(model.state(), QString("ready"));
        QVERIFY(model.shortcuts().value("TriggerKeys").toStringList().isEmpty());
    }
    void editorValidation_data() {
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("result");
        QTest::newRow("duplicate-action") << "Super+space" << "duplicate";
        QTest::newRow("duplicate-reordered") << "Super+Shift+space" << "duplicate";
        QTest::newRow("duplicate-in-list") << "Alt+Control+v\nControl+Alt+v" << "duplicate";
        QTest::newRow("unknown-name") << "Control+notakey" << "invalid";
        QTest::newRow("repeated-modifier") << "Control+Control+v" << "invalid";
        QTest::newRow("space") << " Control+space" << "invalid";
        QTest::newRow("newline") << "F6\n" << "invalid";
    }
    void editorValidation() {
        QFETCH(QString, target); QFETCH(QString, result);
        HotkeyOverview model(*client);
        ready(model);
        model.apply("TriggerKeys", target);
        QCOMPARE(model.state(), result);
        QCOMPARE(mock->writes, 0);
    }
    void editorConflictAndUnconfirmed() {
        HotkeyOverview model(*client);
        ready(model);
        mock->hotkeys["ActivateKeys"] = QVariantMap{{"0", "F10"}};
        model.apply("TriggerKeys", "F6");
        QTRY_COMPARE(model.state(), QString("conflict"));
        QCOMPARE(mock->writes, 0);
        ready(model);
        mock->allowWrite = true;
        mock->ignoreWrite = true;
        mock->expectedWrites = 1;
        model.apply("TriggerKeys", "F6");
        QTRY_COMPARE(model.state(), QString("unconfirmed"));
        QCOMPARE(mock->writes, 1);
    }
    void initialAndValid() {
        HotkeyOverview model(*client);
        QCOMPARE(model.state(), QStringLiteral("unavailable"));
        QVERIFY(model.shortcuts().isEmpty());
        QCOMPARE(mock->reads, 0);
        QSignalSpy changed(&model, &HotkeyOverview::changed);
        ready(model);
        QCOMPARE(changed.size(), 2);
        QCOMPARE(model.shortcuts()["TriggerKeys"].toStringList(), (QStringList{"Control+space", "Zenkaku_Hankaku", "Hangul"}));
        QVERIFY(model.shortcuts()["EnumerateForwardKeys"].toStringList().isEmpty());
        QCOMPARE(model.shortcuts()["EnumerateGroupBackwardKeys"].toStringList(), QStringList{"Shift+Super+space"});
        mock->hotkeys["TriggerKeys"] = indexed(12);
        mock->hotkeys["AltTriggerKeys"] = QVariantMap{};
        mock->hotkeys["Ignored"] = QVariantList{1, 2};
        ready(model);
        const auto list = model.shortcuts()["TriggerKeys"].toStringList();
        QCOMPARE(list.size(), 12);
        QCOMPARE(list[2], QStringLiteral("Key2"));
        QCOMPARE(list[10], QStringLiteral("Key10"));
        QVERIFY(model.shortcuts()["AltTriggerKeys"].toStringList().isEmpty());
        mock->hotkeys["TriggerKeys"] = indexed(32);
        ready(model);
        QCOMPARE(model.shortcuts()["TriggerKeys"].toStringList().size(), 32);
    }
    void malformed_data() {
        QTest::addColumn<QString>("kind");
        for (const auto* kind : {"missing-global", "missing-association", "wrong-association", "duplicate-association",
                "missing-type", "duplicate-type", "missing-field", "duplicate-field", "wrong-field-type",
                "too-many-types", "too-many-options", "missing-runtime", "wrong-root", "wrong-hotkey",
                "string-list", "nonempty-string", "missing-index", "leading-zero", "negative-index", "extra-index",
                "non-string", "empty-item", "long-item", "control-item", "too-many-items", "signature", "dbus-error"})
            QTest::newRow(kind) << QString::fromLatin1(kind);
    }
    void malformed() {
        QFETCH(QString, kind);
        HotkeyOverview model(*client);
        ready(model); // Every failure must also clear an earlier valid snapshot.
        if (kind == "missing-global") mock->types.removeFirst();
        else if (kind == "missing-association") mock->types[0].options.clear();
        else if (kind == "wrong-association") mock->types[0].options[0].type = "OtherType";
        else if (kind == "duplicate-association") mock->types[0].options.append(mock->types[0].options[0]);
        else if (kind == "missing-type") mock->types.removeLast();
        else if (kind == "duplicate-type") mock->types.append(mock->types[1]);
        else if (kind == "missing-field") mock->types[1].options.removeLast();
        else if (kind == "duplicate-field") mock->types[1].options.append(mock->types[1].options[0]);
        else if (kind == "wrong-field-type") mock->types[1].options[0].type = "Key";
        else if (kind == "too-many-types") while (mock->types.size() < 33) mock->types.append(HotkeyWire::Type{"Other", {}});
        else if (kind == "too-many-options") while (mock->types[1].options.size() < 129) mock->types[1].options.append(option("Other", "Key"));
        else if (kind == "missing-runtime") mock->hotkeys.remove("TriggerKeys");
        else if (kind == "wrong-root") mock->rawOverride = QStringLiteral("");
        else if (kind == "wrong-hotkey") mock->rawOverride = QVariantMap{{"Hotkey", ""}};
        else if (kind == "string-list") mock->hotkeys["TriggerKeys"] = QStringList{"Control+space"};
        else if (kind == "nonempty-string") mock->hotkeys["TriggerKeys"] = "Control+space";
        else if (kind == "missing-index") mock->hotkeys["TriggerKeys"] = QVariantMap{{"1", "Key"}};
        else if (kind == "leading-zero") mock->hotkeys["TriggerKeys"] = QVariantMap{{"00", "Key"}};
        else if (kind == "negative-index") mock->hotkeys["TriggerKeys"] = QVariantMap{{"-1", "Key"}};
        else if (kind == "extra-index") mock->hotkeys["TriggerKeys"] = QVariantMap{{"0", "Key"}, {"Extra", "Key"}};
        else if (kind == "non-string") mock->hotkeys["TriggerKeys"] = QVariantMap{{"0", 42}};
        else if (kind == "empty-item") mock->hotkeys["TriggerKeys"] = QVariantMap{{"0", ""}};
        else if (kind == "long-item") mock->hotkeys["TriggerKeys"] = QVariantMap{{"0", QString(129, 'a')}};
        else if (kind == "control-item") mock->hotkeys["TriggerKeys"] = QVariantMap{{"0", "Key\n"}};
        else if (kind == "too-many-items") mock->hotkeys["TriggerKeys"] = indexed(33);
        else if (kind == "signature") mock->wrongSignature = true;
        else if (kind == "dbus-error") mock->fail = true;
        model.reload();
        QCOMPARE(model.state(), QStringLiteral("loading"));
        QVERIFY(model.shortcuts().isEmpty());
        QTRY_COMPARE(model.state(), QStringLiteral("error"));
        QVERIFY(model.shortcuts().isEmpty());
    }
    void busyAndTimeout() {
        HotkeyOverview model(*client);
        mock->hold = true;
        QElapsedTimer elapsed;
        elapsed.start();
        model.reload();
        QTRY_COMPARE(mock->pending.size(), 1);
        for (int i = 0; i < 5; ++i) model.reload();
        QCOMPARE(mock->reads, 1);
        QTRY_COMPARE_WITH_TIMEOUT(model.state(), QStringLiteral("error"), 2500);
        QVERIFY(elapsed.elapsed() >= 1000);
        QVERIFY(elapsed.elapsed() < 2500);
        QVERIFY(model.shortcuts().isEmpty());
        mock->release();
        QTest::qWait(50);
        QCOMPARE(model.state(), QStringLiteral("error"));
        ready(model);
    }
    void ownerReleaseAndMissingOwner() {
        HotkeyOverview model(*client);
        ready(model);
        QVERIFY(server->unregisterService(service));
        QTRY_COMPARE(model.state(), QStringLiteral("unavailable"));
        QVERIFY(model.shortcuts().isEmpty());
        model.reload();
        QTRY_COMPARE(model.state(), QStringLiteral("unavailable"));
        QCOMPARE(mock->reads, 1);
        QVERIFY(server->registerService(service));
        QTest::qWait(50);
        QCOMPARE(model.state(), QStringLiteral("unavailable"));
        QCOMPARE(mock->reads, 1); // Acquisition must not trigger a read.
        ready(model);
    }
    void replacementAndStaleCallback() {
        HotkeyOverview model(*client);
        mock->hold = true;
        model.reload();
        QTRY_COMPARE(mock->pending.size(), 1);
        auto replacement = QDBusConnection::connectToBus(qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), "hotkey-replacement");
        ControllerMock next(replacement);
        next.hotkeys["TriggerKeys"] = QVariantMap{{"0", "NewOwnerKey"}};
        QVERIFY(replacement.registerVirtualObject(path, &next));
        QVERIFY(server->unregisterService(service));
        QVERIFY(replacement.registerService(service));
        QTRY_COMPARE(model.state(), QStringLiteral("unavailable"));
        QTest::qWait(50); // Drain both owner-change signals before explicitly reloading.
        QCOMPARE(next.reads, 0);
        next.hold = true;
        model.reload();
        QTRY_COMPARE(next.pending.size(), 1);
        mock->release(); // Old owner's reply arrives during a newer generation.
        QTest::qWait(50);
        QCOMPARE(model.state(), QStringLiteral("loading"));
        QVERIFY(model.shortcuts().isEmpty());
        next.release();
        QTRY_COMPARE(model.state(), QStringLiteral("ready"));
        QCOMPARE(model.shortcuts()["TriggerKeys"].toStringList(), QStringList{"NewOwnerKey"});
        QCOMPARE(next.writes, 0);
        QVERIFY(next.violations.isEmpty());
        replacement.unregisterService(service);
        replacement.unregisterObject(path);
        QDBusConnection::disconnectFromBus("hotkey-replacement");
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("OPENKEY_HOTKEY_PRIVATE_BUS")) {
        QProcess runner;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("OPENKEY_HOTKEY_PRIVATE_BUS"), QStringLiteral("1"));
        environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_BUS_TYPE"));
        runner.setProcessEnvironment(environment);
        runner.setProcessChannelMode(QProcess::ForwardedChannels);
        runner.start(QStringLiteral("dbus-run-session"), QStringList{QStringLiteral("--"),
            QCoreApplication::applicationFilePath()} + QCoreApplication::arguments().mid(1));
        if (!runner.waitForStarted()) return 1;
        runner.waitForFinished(-1);
        return runner.exitStatus() == QProcess::NormalExit ? runner.exitCode() : 1;
    }
    LinuxHotkeyOverviewTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "LinuxHotkeyOverviewTests.moc"
