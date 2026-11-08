// No desktop bus or configuration files: main re-executes on a fresh private bus.
// CMake may also wrap this executable in dbus-run-session; nesting is harmless.
#include "SettingsController.h"
#include "InputConfigBackup.h"

#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusVariant>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

// Fcitx's schema is a(sa(sssva{sv})), not a QVariantList (which is av).
namespace SettingsWire {
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
Q_DECLARE_METATYPE(SettingsWire::Option)
Q_DECLARE_METATYPE(SettingsWire::Options)
Q_DECLARE_METATYPE(SettingsWire::Type)
Q_DECLARE_METATYPE(SettingsWire::Types)

namespace {
const QString service = QStringLiteral("org.fcitx.Fcitx5");
const QString path = QStringLiteral("/controller");
const QString interface = QStringLiteral("org.fcitx.Fcitx.Controller1");
const QString uri = QStringLiteral("fcitx://config/addon/openkey");
const QString serverName = QStringLiteral("openkey-settings-test-server");
const QString clientName = QStringLiteral("openkey-settings-test-client");

QVariantMap variantMap(const QVariant& value) {
    if (value.metaType() == QMetaType::fromType<QDBusArgument>())
        return qdbus_cast<QVariantMap>(value.value<QDBusArgument>());
    return value.toMap();
}

class ControllerMock : public QDBusVirtualObject {
public:
    explicit ControllerMock(const QDBusConnection& bus) : bus_(bus) {}

    QString runtime = QStringLiteral("Preedit");
    QString diagnostics = QStringLiteral("True");
    QVariant rawDiagnostics; // Optional malformed runtime value for strict decoding tests.
    // Legacy addon by default: neither optional runtime nor schema entries exist.
    QVariantMap optionalRuntime;
    SettingsWire::Options optionalSchema;
    SettingsWire::Types extraSchema;
    QVariantMap changesAfterWrite;
    QString styleType = QStringLiteral("Enum");
    QString diagnosticsType = QStringLiteral("Boolean");
    QVariantMap enumValues{{QStringLiteral("0"), QStringLiteral("Preedit")},
                           {QStringLiteral("1"), QStringLiteral("Direct")}};
    bool includeStyle = true, includeDiagnostics = true;
    bool failReads = false, failReadsAfterWrite = false;
    bool holdWrites = false;
    QList<QDBusMessage> pendingWrites;
    bool failWrite = false, ignoreApply = false, holdReads = false, holdReadback = false;
    int reads = 0, writes = 0;
    QVariantMap lastPatch;
    bool allowBatch = false;
    QStringList calls, violations;
    QList<QDBusMessage> pendingReads;

    QString introspect(const QString&) const override {
        return QStringLiteral(
            "<interface name=\"org.fcitx.Fcitx.Controller1\">"
            "<method name=\"GetConfig\"><arg direction=\"in\" type=\"s\"/>"
            "<arg direction=\"out\" type=\"v\"/>"
            "<arg direction=\"out\" type=\"a(sa(sssva{sv}))\"/></method>"
            "<method name=\"SetConfig\"><arg direction=\"in\" type=\"s\"/>"
            "<arg direction=\"in\" type=\"v\"/></method></interface>");
    }

    SettingsWire::Types schema() const {
        SettingsWire::Options options;
        if (includeStyle)
            options.append({QStringLiteral("InputStyle"), styleType,
                            QStringLiteral("Input style"),
                            QDBusVariant(QStringLiteral("Preedit")),
                            {{QStringLiteral("Enum"), enumValues}}});
        if (includeDiagnostics)
            options.append({QStringLiteral("DirectDiagnostics"), diagnosticsType,
                            QStringLiteral("Direct diagnostics"),
                            QDBusVariant(QStringLiteral("False")), {}});
        options.append(optionalSchema);
        SettingsWire::Types types{{QStringLiteral("InputConfig"), options}};
        types.append(extraSchema);
        return types;
    }

    void replyToRead(const QDBusMessage& message) {
        if (failReads || (failReadsAfterWrite && writes > 0)) {
            bus_.send(message.createErrorReply(QDBusError::Failed, QStringLiteral("Test read failure")));
            return;
        }
        QVariantMap raw{{QStringLiteral("InputStyle"), runtime},
                              {QStringLiteral("DirectDiagnostics"),
                               rawDiagnostics.isValid() ? rawDiagnostics : QVariant(diagnostics)}};
        raw.insert(optionalRuntime);
        bus_.send(message.createReply(QVariantList{
            QVariant::fromValue(QDBusVariant(raw)), QVariant::fromValue(schema())}));
    }

    void releaseReads() {
        holdReads = false;
        const auto pending = pendingReads;
        pendingReads.clear();
        for (const auto& message : pending) replyToRead(message);
    }

    bool handleMessage(const QDBusMessage& message, const QDBusConnection&) override {
        if (message.interface() != interface) return false;
        const auto args = message.arguments();
        const bool get = message.member() == QStringLiteral("GetConfig");
        const bool set = message.member() == QStringLiteral("SetConfig");
        if (!get && !set) {
            violations.append(QStringLiteral("Unexpected method: ") + message.member());
            bus_.send(message.createErrorReply(QDBusError::UnknownMethod, QStringLiteral("Unexpected method")));
            return true;
        }
        calls.append(message.member());
        if (get) ++reads;
        else ++writes;
        if (message.signature() != (get ? QStringLiteral("s") : QStringLiteral("sv")) ||
            args.isEmpty() || args.first().toString() != uri) {
            violations.append(QStringLiteral("Wrong request signature or URI"));
            bus_.send(message.createErrorReply(QDBusError::InvalidArgs, violations.last()));
            return true;
        }
        if (get) {
            if (holdReads || (holdReadback && writes > 0)) pendingReads.append(message);
            else replyToRead(message);
            return true;
        }
        const QVariant value = args.at(1).value<QDBusVariant>().variant();
        lastPatch = variantMap(value);
        const QVariant style = lastPatch.value(QStringLiteral("InputStyle"));
        const QVariant diagnosticsValue = lastPatch.value(QStringLiteral("DirectDiagnostics"));
        const bool stylePatch = style.metaType() == QMetaType::fromType<QString>() &&
            (style.toString() == QStringLiteral("Preedit") || style.toString() == QStringLiteral("Direct") ||
             (style.toString() == QStringLiteral("CommitOnly") && enumValues.value("2").toString() == "CommitOnly") ||
             (style.toString() == QStringLiteral("NativeDirect") && enumValues.value("3").toString() == "NativeDirect"));
        const bool diagnosticsPatch = diagnosticsValue.metaType() == QMetaType::fromType<QString>() &&
            (diagnosticsValue.toString() == QStringLiteral("True") || diagnosticsValue.toString() == QStringLiteral("False"));
        const QString optionalKey = lastPatch.isEmpty() ? QString() : lastPatch.firstKey();
        const QVariant optionalValue = lastPatch.value(optionalKey);
        const bool optionalPatch = (optionalKey == QStringLiteral("CheckSpelling") ||
                                    optionalKey == QStringLiteral("UseModernOrthography") ||
                                    optionalKey == QStringLiteral("QuickTelex") || optionalKey == QStringLiteral("MacrosEnabled") ||
                                    QStringList{"FreeMark", "AllowConsonantZFWJ", "QuickStartConsonant", "QuickEndConsonant", "RestoreWrongSpelling", "RememberInputMethodPerApplication"}.contains(optionalKey)) &&
            optionalValue.metaType() == QMetaType::fromType<QString>() &&
            (optionalValue.toString() == QStringLiteral("True") || optionalValue.toString() == QStringLiteral("False"));
        bool validBatch = allowBatch && lastPatch.size() >= 2 && lastPatch.size() <= 11;
        if (validBatch) {
            for (auto it = lastPatch.cbegin(); it != lastPatch.cend(); ++it) {
                validBatch = validBatch && it.value().metaType() == QMetaType::fromType<QString>();
                if (it.key() == "InputStyle") validBatch = validBatch && stylePatch;
                else if (it.key() == "DirectDiagnostics") validBatch = validBatch && diagnosticsPatch;
                else validBatch = validBatch && optionalRuntime.contains(it.key()) &&
                    (it.value() == QString("True") || it.value() == QString("False"));
            }
        }
        if (!validBatch && (lastPatch.size() != 1 || (!stylePatch && !diagnosticsPatch && !optionalPatch && !((optionalKey == "MacroTable" || optionalKey == "ApplicationRules") && optionalValue.metaType() == QMetaType::fromType<QString>())))) {
            violations.append(QStringLiteral("SetConfig must contain exactly one known setting string patch"));
            bus_.send(message.createErrorReply(QDBusError::InvalidArgs, violations.last()));
        } else if (failWrite) {
            bus_.send(message.createErrorReply(QDBusError::Failed, QStringLiteral("Test write failure")));
        } else {
            if (!ignoreApply) {
                for (auto it = lastPatch.cbegin(); it != lastPatch.cend(); ++it) {
                    if (it.key() == "InputStyle") runtime = it.value().toString();
                    else if (it.key() == "DirectDiagnostics") diagnostics = it.value().toString();
                    else optionalRuntime[it.key()] = it.value();
                }
            }
            for (auto it = changesAfterWrite.cbegin(); it != changesAfterWrite.cend(); ++it) {
                if (it.key() == QStringLiteral("InputStyle")) runtime = it.value().toString();
                else if (it.key() == QStringLiteral("DirectDiagnostics")) diagnostics = it.value().toString();
                else optionalRuntime[it.key()] = it.value();
            }
            if (holdWrites) pendingWrites.append(message);
            else bus_.send(message.createReply()); // Empty ACK is not proof of application.
        }
        return true;
    }
private:
    QDBusConnection bus_;
};
}

class LinuxSettingsControllerTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QDBusConnection> server_, client_;
    std::unique_ptr<ControllerMock> mock_;

    // Constructor auto-loading is deliberately not required by these tests.
    void load(SettingsController& controller) {
        controller.reload();
        QTRY_COMPARE(controller.result(), QStringLiteral("loaded"));
        QTRY_VERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(controller.runtimeStyle(), mock_->runtime);
        QCOMPARE(controller.runtimeDiagnostics(), mock_->diagnostics);
    }

    void enableSpelling() {
        mock_->optionalRuntime = {{QStringLiteral("CheckSpelling"), QStringLiteral("True")},
                                  {QStringLiteral("UseModernOrthography"), QStringLiteral("False")}};
        mock_->optionalSchema = {
            {QStringLiteral("CheckSpelling"), QStringLiteral("Boolean"), QStringLiteral("Spelling"), QDBusVariant(QStringLiteral("True")), {}},
            {QStringLiteral("UseModernOrthography"), QStringLiteral("Boolean"), QStringLiteral("Modern"), QDBusVariant(QStringLiteral("False")), {}}};
    }

    void enableQuickTelex() {
        mock_->optionalRuntime[QStringLiteral("QuickTelex")] = QStringLiteral("True");
        mock_->optionalSchema.append({QStringLiteral("QuickTelex"), QStringLiteral("Boolean"),
            QStringLiteral("Quick Telex"), QDBusVariant(QStringLiteral("False")), {}});
    }

    void applyField(SettingsController& controller, const QString& key, const QString& target) {
        if (key == QStringLiteral("InputStyle")) controller.apply(target);
        else if (key == QStringLiteral("DirectDiagnostics")) controller.applyDiagnostics(target);
        else if (key == QStringLiteral("CheckSpelling")) controller.applySpelling(target);
        else if (key == QStringLiteral("QuickTelex")) controller.applyQuickTelex(target);
        else controller.applyModernOrthography(target);
    }

    void verifyCleared(const SettingsController& controller) {
        QVERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(!controller.spellingSupported());
        QVERIFY(!controller.quickTelexSupported());
        QVERIFY(controller.runtimeQuickTelex().isEmpty());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QVERIFY(controller.runtimeCheckSpelling().isEmpty());
        QVERIFY(controller.runtimeModernOrthography().isEmpty());
    }

private slots:
    void advancedOptions() {
        const QStringList fields{"FreeMark", "AllowConsonantZFWJ", "QuickStartConsonant", "QuickEndConsonant", "RestoreWrongSpelling", "RememberInputMethodPerApplication"};
        for (const auto& key : fields) {
            mock_->optionalRuntime[key] = "False";
            mock_->optionalSchema.append({key, "Boolean", key, QDBusVariant(QString("False")), {}});
        }
        SettingsController controller(*client_);
        load(controller);
        QCOMPARE(controller.runtimeAdvanced().size(), 6);
        for (const auto& key : fields) {
            controller.applyAdvanced(key, "True"); QTRY_COMPARE(controller.result(), QString("confirmed"));
            QCOMPARE(controller.runtimeAdvanced().value(key).toString(), "True");
        }
        QCOMPARE(mock_->writes, 6);
        const auto backup = controller.backupJson();
        QVERIFY(backup.contains("QuickEndConsonant"));
        QVERIFY(backup.contains("RememberInputMethodPerApplication"));
        controller.applyAdvanced("Unknown", "True"); QCOMPARE(controller.result(), QString("invalid"));
        QCOMPARE(mock_->writes, 6);
    }
    void applicationRulesApply() {
        mock_->optionalRuntime = {{"ApplicationRules", ""}};
        mock_->optionalSchema = {{"ApplicationRules", "String", "App rules", QDBusVariant(QString("")), {}}};
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(controller.applicationRulesSupported());
        controller.applyApplicationRules("636f6465:50726565646974;"); // code -> Preedit
        QTRY_COMPARE(controller.result(), QString("confirmed"));
        QCOMPARE(mock_->writes, 1);
        controller.applyApplicationRules("676e6f6d652d7368656c6c:446972656374;");
        QCOMPARE(controller.result(), QString("invalid"));
        QCOMPARE(mock_->writes, 1);
    }
    void macrosApplyAndConflict() {
        mock_->optionalRuntime = {{"MacrosEnabled", "False"}, {"MacroTable", ""}};
        mock_->optionalSchema = {
            {"MacrosEnabled", "Boolean", "Macros", QDBusVariant(QString("False")), {}},
            {"MacroTable", "String", "Table", QDBusVariant(QString("")), {}}};
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(controller.macrosSupported());
        controller.applyMacroTable("766e:5669e1bb8774204e616d;");
        QTRY_COMPARE(controller.result(), QString("confirmed"));
        QCOMPARE(mock_->writes, 1);
        QCOMPARE(controller.runtimeMacroTable(), QString("766e:5669e1bb8774204e616d;"));
        controller.applyMacrosEnabled("True");
        QTRY_COMPARE(controller.result(), QString("confirmed"));
        QCOMPARE(controller.runtimeMacrosEnabled(), QString("True"));
        QCOMPARE(mock_->writes, 2);
        controller.applyMacroTable("bad");
        QCOMPARE(controller.result(), QString("invalid"));
        QCOMPARE(mock_->writes, 2);
        load(controller);
        mock_->optionalRuntime["MacroTable"] = "61:62;";
        controller.applyMacroTable("");
        QTRY_COMPARE(controller.result(), QString("conflict"));
        QCOMPARE(mock_->writes, 2);
    }
    void initTestCase() {
        QVERIFY2(qEnvironmentVariableIsSet("OPENKEY_SETTINGS_TEST_PRIVATE_BUS"),
                 "Tests must run in the private dbus-run-session child");
        QVERIFY(!qgetenv("DBUS_SESSION_BUS_ADDRESS").isEmpty());
        qDBusRegisterMetaType<SettingsWire::Option>();
        qDBusRegisterMetaType<SettingsWire::Options>();
        qDBusRegisterMetaType<SettingsWire::Type>();
        qDBusRegisterMetaType<SettingsWire::Types>();
    }

    void init() {
        const QString address = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        server_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, serverName));
        client_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, clientName));
        QVERIFY(server_->isConnected());
        QVERIFY(client_->isConnected());
        QVERIFY(server_->baseService() != client_->baseService());
        mock_ = std::make_unique<ControllerMock>(*server_);
        QVERIFY(server_->registerVirtualObject(path, mock_.get()));
        QVERIFY(server_->registerService(service));
    }

    void cleanup() {
        const QString violations = mock_ ? mock_->violations.join(QLatin1Char('\n')) : QString();
        if (server_) {
            server_->unregisterService(service);
            server_->unregisterObject(path);
        }
        mock_.reset();
        client_.reset();
        server_.reset();
        QDBusConnection::disconnectFromBus(clientName);
        QDBusConnection::disconnectFromBus(serverName);
        QVERIFY2(violations.isEmpty(), qPrintable(violations));
    }

    void loadRuntime_data() {
        QTest::addColumn<QString>("style");
        QTest::addColumn<QString>("diagnostics");
        QTest::newRow("preedit-on") << QStringLiteral("Preedit") << QStringLiteral("True");
        QTest::newRow("preedit-off") << QStringLiteral("Preedit") << QStringLiteral("False");
        QTest::newRow("direct-on") << QStringLiteral("Direct") << QStringLiteral("True");
        QTest::newRow("direct-off") << QStringLiteral("Direct") << QStringLiteral("False");
    }
    void loadRuntime() {
        QFETCH(QString, style);
        QFETCH(QString, diagnostics);
        mock_->runtime = style;
        mock_->diagnostics = diagnostics;
        SettingsController controller(*client_);
        QSignalSpy changed(&controller, &SettingsController::changed);
        QVERIFY(changed.isValid());
        load(controller);
        QVERIFY(controller.ready());
        QVERIFY(!changed.isEmpty());
        QVERIFY(!controller.spellingSupported());
        QVERIFY(controller.runtimeCheckSpelling().isEmpty());
        QVERIFY(controller.runtimeModernOrthography().isEmpty());
        QVERIFY(mock_->reads >= 1);
        QCOMPARE(mock_->writes, 0);
    }

    void inputBackupRoundtrip() {
        enableSpelling();
        enableQuickTelex();
        mock_->enumValues.insert("2", "CommitOnly");
        mock_->enumValues.insert("3", "NativeDirect");
        SettingsController controller(*client_);
        load(controller);
        const auto json = controller.backupJson();
        QVERIFY(!json.isEmpty());
        QVariantMap patch;
        QVERIFY(InputConfigBackup::decode(json, patch));
        QCOMPARE(patch.size(), 5);
        QCOMPARE(mock_->writes, 0);
        mock_->runtime = "NativeDirect";
        mock_->diagnostics = "False";
        mock_->optionalRuntime["QuickTelex"] = "False";
        load(controller);
        const auto preview = controller.inspectBackup(json);
        QVERIFY(preview.value("ok").toBool());
        QCOMPARE(mock_->writes, 0);
        mock_->allowBatch = true;
        controller.restoreBackup(json);
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QString("confirmed"));
        QCOMPARE(controller.backupJson(), json);
        QCOMPARE(mock_->writes, 1);
        QCOMPARE(mock_->lastPatch, patch);
        // Confirmation is single-use, even if the same text is submitted again.
        controller.restoreBackup(json);
        QCOMPARE(controller.result(), QString("invalid"));
        QCOMPARE(mock_->writes, 1);
    }

    void invalidInputBackup_data() {
        QTest::addColumn<QString>("json");
        const QString good = InputConfigBackup::encode({{"InputStyle", "Direct"}, {"DirectDiagnostics", "True"}});
        QTest::newRow("empty") << QString();
        QTest::newRow("array") << QString("[]");
        QTest::newRow("oversized") << QString(4097, ' ');
        QTest::newRow("ui-format") << QString(good).replace("openkey-input-settings", "openkey-ui-settings");
        QTest::newRow("wrong-version") << QString(good).replace("\"version\": 1", "\"version\": 2");
        QTest::newRow("wrong-type") << QString(good).replace("true", "\"True\"");
        QTest::newRow("unknown-style") << QString(good).replace("Direct\"", "Invalid\"");
        QTest::newRow("missing-field") << QString("{\"format\":\"openkey-input-settings\",\"version\":1,\"settings\":{\"InputStyle\":\"Direct\"}}");
        QTest::newRow("unknown-field") << QString(good).replace("\"InputStyle\"", "\"RunCommand\"");
        QTest::newRow("root-field") << QString(good).replace("\"version\": 1", "\"extra\": true, \"version\": 1");
        QTest::newRow("unpaired-spelling") << QString(good).replace("\"InputStyle\"", "\"CheckSpelling\":true, \"InputStyle\"");
    }
    void invalidInputBackup() {
        QFETCH(QString, json);
        SettingsController controller(*client_);
        load(controller);
        QVariantMap patch{{"old", "value"}};
        QVERIFY(!InputConfigBackup::decode(json, patch));
        QVERIFY(patch.isEmpty());
        QVERIFY(!controller.inspectBackup(json).value("ok").toBool());
        controller.restoreBackup(json);
        QCOMPARE(controller.result(), QString("invalid"));
        QCOMPARE(mock_->writes, 0);
    }

    void inputBackupFailure_data() {
        QTest::addColumn<QString>("scenario");
        for (const auto* scenario : {"conflict", "unsupported", "changed-json", "not-inspected", "reloaded",
                                    "ignored", "write-error", "no-op", "busy", "readback-mismatch", "owner-loss",
                                    "write-timeout", "readback-timeout", "preflight-timeout", "support-lost"})
            QTest::newRow(scenario) << QString::fromLatin1(scenario);
    }
    void inputBackupFailure() {
        QFETCH(QString, scenario);
        SettingsController controller(*client_);
        load(controller);
        const auto json = InputConfigBackup::encode({{"InputStyle", "Direct"}, {"DirectDiagnostics", "False"}});
        QVERIFY(controller.inspectBackup(json).value("ok").toBool());
        QString expected = "invalid";
        int writes = 0;
        if (scenario == "conflict") { mock_->diagnostics = "False"; expected = "conflict"; }
        else if (scenario == "preflight-timeout") { mock_->holdReads = true; expected = "error"; }
        else if (scenario == "write-timeout" || scenario == "readback-timeout") {
            mock_->holdWrites = scenario == "write-timeout";
            mock_->holdReadback = scenario == "readback-timeout";
            writes = 1; expected = "unconfirmed";
        } else if (scenario == "support-lost") {
            mock_->enumValues.insert("2", "CommitOnly");
            mock_->enumValues.insert("3", "NativeDirect");
            load(controller);
            const auto newer = InputConfigBackup::encode({{"InputStyle", "NativeDirect"}, {"DirectDiagnostics", "False"}});
            QVERIFY(controller.inspectBackup(newer).value("ok").toBool());
            mock_->enumValues.remove("3");
            controller.restoreBackup(newer);
            QTRY_VERIFY(!controller.busy());
            QCOMPARE(controller.result(), QString("unsupported"));
            QCOMPARE(mock_->writes, 0);
            return;
        }
        else if (scenario == "unsupported") {
            const auto newer = InputConfigBackup::encode({{"InputStyle", "NativeDirect"}, {"DirectDiagnostics", "False"}});
            QCOMPARE(controller.inspectBackup(newer).value("status").toString(), QString("unsupported"));
        } else if (scenario == "not-inspected") {
            SettingsController fresh(*client_);
            load(fresh);
            fresh.restoreBackup(json);
            QCOMPARE(fresh.result(), QString("invalid"));
            QCOMPARE(mock_->writes, 0);
            return;
        } else if (scenario == "reloaded") load(controller);
        else if (scenario == "ignored" || scenario == "write-error" || scenario == "readback-mismatch") {
            mock_->ignoreApply = scenario == "ignored";
            mock_->failWrite = scenario == "write-error";
            if (scenario == "readback-mismatch") mock_->changesAfterWrite["DirectDiagnostics"] = "True";
            writes = 1; expected = "unconfirmed";
        } else if (scenario == "no-op") {
            const auto same = controller.backupJson();
            QVERIFY(controller.inspectBackup(same).value("ok").toBool());
            controller.restoreBackup(same);
            QTRY_VERIFY(!controller.busy());
            QCOMPARE(controller.result(), QString("confirmed"));
            QCOMPARE(mock_->writes, 0);
            return;
        } else if (scenario == "busy") {
            mock_->holdReads = true;
            controller.reload();
            controller.restoreBackup(json);
            QCOMPARE(mock_->writes, 0);
            mock_->holdReads = false;
            QTRY_VERIFY(!controller.busy());
            return;
        } else if (scenario == "owner-loss") {
            server_->unregisterService(service);
            QTRY_VERIFY(!controller.ready());
            controller.restoreBackup(json);
            QCOMPARE(controller.result(), QString("unavailable"));
            QCOMPARE(mock_->writes, 0);
            return;
        }
        mock_->allowBatch = true;
        controller.restoreBackup(scenario == "changed-json" ? json + " " : json);
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), expected);
        QCOMPARE(mock_->writes, writes);
    }

    void reloadNeverWrites() {
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->runtime = QStringLiteral("Direct");
        mock_->diagnostics = QStringLiteral("False");
        controller.reload();
        QTRY_VERIFY(mock_->reads > before);
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("loaded"));
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
        QCOMPARE(controller.runtimeDiagnostics(), QStringLiteral("False"));
        QVERIFY(controller.ready());
        QCOMPARE(mock_->writes, 0);
    }

    void nativeDirectRequiresSchemaSupport() {
        SettingsController controller(*client_);
        for (const bool commitOnly : {false, true}) {
            if (commitOnly) mock_->enumValues.insert(QStringLiteral("2"), QStringLiteral("CommitOnly"));
            load(controller);
            QVERIFY(controller.ready());
            QVERIFY(!controller.nativeDirectSupported());
            controller.apply(QStringLiteral("NativeDirect"));
            QCOMPARE(controller.result(), QStringLiteral("unsupported"));
            QCOMPARE(mock_->writes, 0);
        }
        mock_->enumValues.insert(QStringLiteral("3"), QStringLiteral("NativeDirect"));
        load(controller);
        QVERIFY(controller.nativeDirectSupported());
        QVERIFY(controller.commitOnlySupported());
        controller.apply(QStringLiteral("NativeDirect"));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("NativeDirect"));
        QCOMPARE(mock_->lastPatch, (QVariantMap{{QStringLiteral("InputStyle"), QStringLiteral("NativeDirect")}}));
        controller.apply(QStringLiteral("Direct"));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
    }

    void commitOnlyRequiresSchemaSupport() {
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(!controller.commitOnlySupported());
        controller.apply(QStringLiteral("CommitOnly"));
        QCOMPARE(controller.result(), QStringLiteral("unsupported"));
        QCOMPARE(mock_->writes, 0);
        mock_->enumValues.insert(QStringLiteral("2"), QStringLiteral("CommitOnly"));
        load(controller);
        QVERIFY(controller.ready());
        QVERIFY(controller.commitOnlySupported());
        controller.apply(QStringLiteral("CommitOnly"));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("CommitOnly"));
        QCOMPARE(mock_->lastPatch.size(), 1);
        QCOMPARE(mock_->lastPatch.value(QStringLiteral("InputStyle")).toString(), QStringLiteral("CommitOnly"));
        controller.apply(QStringLiteral("Preedit"));
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Preedit"));
    }

    void applyConfirmsReadback_data() {
        QTest::addColumn<QString>("initial");
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("diagnostics");
        QTest::newRow("enable-direct-diagnostics-on")
            << QStringLiteral("Preedit") << QStringLiteral("Direct") << QStringLiteral("True");
        QTest::newRow("enable-direct-diagnostics-off")
            << QStringLiteral("Preedit") << QStringLiteral("Direct") << QStringLiteral("False");
        QTest::newRow("restore-preedit-diagnostics-on")
            << QStringLiteral("Direct") << QStringLiteral("Preedit") << QStringLiteral("True");
        QTest::newRow("restore-preedit-diagnostics-off")
            << QStringLiteral("Direct") << QStringLiteral("Preedit") << QStringLiteral("False");
    }
    void applyConfirmsReadback() {
        QFETCH(QString, initial);
        QFETCH(QString, target);
        QFETCH(QString, diagnostics);
        mock_->runtime = initial;
        mock_->diagnostics = diagnostics;
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->calls.clear();
        QSignalSpy changed(&controller, &SettingsController::changed);
        controller.apply(target);
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(controller.runtimeStyle(), target);
        QCOMPARE(mock_->runtime, target);
        QCOMPARE(mock_->writes, 1);
        QVERIFY(mock_->reads >= before + 2); // Fresh preflight and post-ACK readback.
        QCOMPARE(mock_->lastPatch, (QVariantMap{{QStringLiteral("InputStyle"), target}}));
        const int writeIndex = mock_->calls.indexOf(QStringLiteral("SetConfig"));
        QVERIFY(writeIndex > 0);
        QCOMPARE(mock_->calls.first(), QStringLiteral("GetConfig"));
        QVERIFY(mock_->calls.mid(writeIndex + 1).contains(QStringLiteral("GetConfig")));
        QVERIFY(!changed.isEmpty());
        QCOMPARE(mock_->diagnostics, diagnostics);
        QCOMPARE(controller.runtimeDiagnostics(), diagnostics);
    }

    void applyDiagnosticsConfirmsReadback_data() {
        QTest::addColumn<QString>("initial");
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("style");
        QTest::newRow("disable-preedit") << QStringLiteral("True") << QStringLiteral("False") << QStringLiteral("Preedit");
        QTest::newRow("enable-preedit") << QStringLiteral("False") << QStringLiteral("True") << QStringLiteral("Preedit");
        QTest::newRow("disable-direct") << QStringLiteral("True") << QStringLiteral("False") << QStringLiteral("Direct");
        QTest::newRow("enable-direct") << QStringLiteral("False") << QStringLiteral("True") << QStringLiteral("Direct");
    }
    void applyDiagnosticsConfirmsReadback() {
        QFETCH(QString, initial);
        QFETCH(QString, target);
        QFETCH(QString, style);
        mock_->runtime = style;
        mock_->diagnostics = initial;
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->calls.clear();
        QSignalSpy changed(&controller, &SettingsController::changed);
        controller.applyDiagnostics(target);
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(controller.runtimeDiagnostics(), target);
        QCOMPARE(controller.runtimeStyle(), style);
        QCOMPARE(mock_->diagnostics, target);
        QCOMPARE(mock_->runtime, style);
        QCOMPARE(mock_->writes, 1);
        QVERIFY(mock_->reads >= before + 2);
        QCOMPARE(mock_->lastPatch, (QVariantMap{{QStringLiteral("DirectDiagnostics"), target}}));
        QCOMPARE(mock_->lastPatch.value(QStringLiteral("DirectDiagnostics")).metaType(), QMetaType::fromType<QString>());
        const int writeIndex = mock_->calls.indexOf(QStringLiteral("SetConfig"));
        QVERIFY(writeIndex > 0);
        QCOMPARE(mock_->calls.first(), QStringLiteral("GetConfig"));
        QVERIFY(mock_->calls.mid(writeIndex + 1).contains(QStringLiteral("GetConfig")));
        QVERIFY(!changed.isEmpty());
    }

    void sameDiagnosticsConfirmsWithoutWrite() {
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->runtime = QStringLiteral("Direct");
        controller.applyDiagnostics(QStringLiteral("True"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QVERIFY(mock_->reads > before);
        QCOMPARE(controller.runtimeDiagnostics(), QStringLiteral("True"));
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
        QCOMPARE(mock_->writes, 0);
    }

    void sameStyleConfirmsWithoutWrite() {
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        controller.apply(QStringLiteral("Preedit"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QVERIFY(mock_->reads > before); // Still check the baseline against runtime.
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Preedit"));
        QCOMPARE(mock_->writes, 0);
    }

    void invalidStyleNeverWrites_data() {
        QTest::addColumn<QString>("style");
        QTest::newRow("empty") << QString();
        QTest::newRow("wrong-case") << QStringLiteral("direct");
        QTest::newRow("whitespace") << QStringLiteral(" Direct ");
        QTest::newRow("unknown") << QStringLiteral("Other");
    }
    void invalidStyleNeverWrites() {
        QFETCH(QString, style);
        SettingsController controller(*client_);
        load(controller);
        controller.apply(style);
        QTRY_COMPARE(controller.result(), QStringLiteral("invalid"));
        QVERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QCOMPARE(mock_->writes, 0);
        QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
    }

    void invalidDiagnosticsNeverWrites_data() {
        QTest::addColumn<QString>("target");
        QTest::newRow("empty") << QString();
        QTest::newRow("lowercase-true") << QStringLiteral("true");
        QTest::newRow("lowercase-false") << QStringLiteral("false");
        QTest::newRow("whitespace") << QStringLiteral(" True ");
        QTest::newRow("numeric") << QStringLiteral("1");
        QTest::newRow("style") << QStringLiteral("Direct");
    }
    void invalidDiagnosticsNeverWrites() {
        QFETCH(QString, target);
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        controller.applyDiagnostics(target);
        QTRY_COMPARE(controller.result(), QStringLiteral("invalid"));
        QVERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QCOMPARE(mock_->reads, before);
        QCOMPARE(mock_->writes, 0);
        QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
        QCOMPARE(mock_->diagnostics, QStringLiteral("True"));
    }

    void busyReloadRejectsAdditionalRequests() {
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->holdReads = true;
        controller.reload();
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("loading"));
        controller.reload();
        controller.apply(QStringLiteral("Direct"));
        controller.applyDiagnostics(QStringLiteral("False"));
        controller.applySpelling(QStringLiteral("False"));
        controller.applyModernOrthography(QStringLiteral("True"));
        controller.applyQuickTelex(QStringLiteral("False"));
        QTest::qWait(30);
        QCOMPARE(mock_->reads, before + 1);
        QCOMPARE(mock_->writes, 0);
        mock_->releaseReads();
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("loaded"));
        QTest::qWait(30); // Rejected actions must not be queued for later.
        QCOMPARE(mock_->reads, before + 1);
        QCOMPARE(mock_->writes, 0);
    }

    void busyApplyRejectsAdditionalRequests() {
        SettingsController controller(*client_);
        load(controller);
        mock_->holdReads = true;
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("applying"));
        const int before = mock_->reads;
        controller.apply(QStringLiteral("Preedit"));
        controller.applyDiagnostics(QStringLiteral("False"));
        controller.applySpelling(QStringLiteral("False"));
        controller.applyModernOrthography(QStringLiteral("True"));
        controller.applyQuickTelex(QStringLiteral("False"));
        controller.reload();
        QTest::qWait(30);
        QCOMPARE(mock_->reads, before);
        QCOMPARE(mock_->writes, 0);
        mock_->releaseReads();
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 1);
        QCOMPARE(mock_->runtime, QStringLiteral("Direct"));
    }

    void runtimeMismatchCancelsBeforeWrite() {
        SettingsController controller(*client_);
        load(controller);
        mock_->runtime = QStringLiteral("Direct"); // Another client changed the loaded snapshot.
        const int before = mock_->reads;
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(controller.result(), QStringLiteral("conflict"));
        QVERIFY(!controller.busy());
        QVERIFY(mock_->reads > before);
        QCOMPARE(mock_->writes, 0);
    }

    void diagnosticsMismatchCancelsBeforeWrite() {
        SettingsController controller(*client_);
        load(controller);
        mock_->diagnostics = QStringLiteral("False");
        const int before = mock_->reads;
        controller.applyDiagnostics(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("conflict"));
        QVERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QVERIFY(mock_->reads > before);
        QCOMPARE(mock_->writes, 0);
    }

    void externalOtherFieldChangeDoesNotConflict_data() {
        QTest::addColumn<bool>("applyDiagnostics");
        QTest::newRow("external-style-change") << true;
        QTest::newRow("external-diagnostics-change") << false;
    }
    void externalOtherFieldChangeDoesNotConflict() {
        QFETCH(bool, applyDiagnostics);
        SettingsController controller(*client_);
        load(controller);
        if (applyDiagnostics) {
            mock_->runtime = QStringLiteral("Direct");
            controller.applyDiagnostics(QStringLiteral("False"));
        } else {
            mock_->diagnostics = QStringLiteral("False");
            controller.apply(QStringLiteral("Direct"));
        }
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(!controller.busy());
        QVERIFY(controller.ready());
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
        QCOMPARE(controller.runtimeDiagnostics(), QStringLiteral("False"));
        QCOMPARE(mock_->runtime, QStringLiteral("Direct"));
        QCOMPARE(mock_->diagnostics, QStringLiteral("False"));
        QCOMPARE(mock_->writes, 1);
        const QString field = applyDiagnostics ? QStringLiteral("DirectDiagnostics") : QStringLiteral("InputStyle");
        const QString target = applyDiagnostics ? QStringLiteral("False") : QStringLiteral("Direct");
        QCOMPARE(mock_->lastPatch, (QVariantMap{{field, target}}));
    }

    void unsuccessfulApply_data() {
        QTest::addColumn<bool>("diagnosticsApply");
        QTest::addColumn<bool>("readError");
        QTest::addColumn<bool>("writeError");
        QTest::addColumn<bool>("ignoreApply");
        QTest::newRow("readback-error") << false << true << false << false;
        QTest::newRow("write-error") << false << false << true << false;
        QTest::newRow("ack-without-change") << false << false << false << true;
        QTest::newRow("diagnostics-readback-error") << true << true << false << false;
        QTest::newRow("diagnostics-write-error") << true << false << true << false;
        QTest::newRow("diagnostics-ack-without-change") << true << false << false << true;
    }
    void unsuccessfulApply() {
        QFETCH(bool, diagnosticsApply);
        QFETCH(bool, readError);
        QFETCH(bool, writeError);
        QFETCH(bool, ignoreApply);
        SettingsController controller(*client_);
        load(controller);
        mock_->failReadsAfterWrite = readError;
        mock_->failWrite = writeError;
        mock_->ignoreApply = ignoreApply;
        const int before = mock_->reads;
        if (diagnosticsApply) controller.applyDiagnostics(QStringLiteral("False"));
        else controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(controller.result(), QStringLiteral("unconfirmed"));
        QVERIFY(!controller.busy());
        QCOMPARE(mock_->writes, 1);
        if (!writeError) QVERIFY(mock_->reads >= before + 2);
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        if (ignoreApply || writeError) QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
        if (ignoreApply || writeError) QCOMPARE(mock_->diagnostics, QStringLiteral("True"));
        const int settledReads = mock_->reads;
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 1); // No automatic write retry.
        QCOMPARE(mock_->reads, settledReads); // No automatic readback retry either.
    }

    void failedReloadDisablesReady() {
        SettingsController controller(*client_);
        load(controller);
        mock_->failReads = true;
        controller.reload();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QCOMPARE(controller.result(), QStringLiteral("error"));
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
        mock_->failReads = false;
        load(controller);
    }

    void invalidSchemaDisablesReady_data() {
        QTest::addColumn<QString>("defect");
        for (const char* defect : {"missing-style", "missing-diagnostics", "style-type",
                                   "diagnostics-type", "diagnostics-bool-type", "enum-missing", "enum-value", "enum-extra"})
            QTest::newRow(defect) << QString::fromLatin1(defect);
    }
    void invalidSchemaDisablesReady() {
        QFETCH(QString, defect);
        SettingsController controller(*client_);
        load(controller);
        if (defect == QStringLiteral("missing-style")) mock_->includeStyle = false;
        if (defect == QStringLiteral("missing-diagnostics")) mock_->includeDiagnostics = false;
        if (defect == QStringLiteral("style-type")) mock_->styleType = QStringLiteral("String");
        if (defect == QStringLiteral("diagnostics-type")) mock_->diagnosticsType = QStringLiteral("String");
        if (defect == QStringLiteral("diagnostics-bool-type")) mock_->diagnosticsType = QStringLiteral("Bool");
        if (defect == QStringLiteral("enum-missing")) mock_->enumValues.remove(QStringLiteral("1"));
        if (defect == QStringLiteral("enum-value")) mock_->enumValues[QStringLiteral("1")] = QStringLiteral("Other");
        if (defect == QStringLiteral("enum-extra")) mock_->enumValues[QStringLiteral("2")] = QStringLiteral("Other");
        const int before = mock_->reads;
        controller.reload();
        QTRY_VERIFY(mock_->reads > before);
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QCOMPARE(controller.result(), QStringLiteral("error"));
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
    }

    void invalidDiagnosticsRuntimeDisablesReady_data() {
        QTest::addColumn<QVariant>("value");
        QTest::newRow("bool-true") << QVariant(true);
        QTest::newRow("bool-false") << QVariant(false);
        QTest::newRow("empty") << QVariant(QString());
        QTest::newRow("lowercase") << QVariant(QStringLiteral("true"));
        QTest::newRow("whitespace") << QVariant(QStringLiteral("False "));
        QTest::newRow("number") << QVariant(1);
    }
    void invalidDiagnosticsRuntimeDisablesReady() {
        QFETCH(QVariant, value);
        SettingsController controller(*client_);
        load(controller);
        mock_->rawDiagnostics = value;
        controller.reload();
        QTRY_COMPARE(controller.result(), QStringLiteral("error"));
        QVERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        controller.applyDiagnostics(QStringLiteral("False"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
    }

    void readTimeoutIgnoresLateReply_data() {
        QTest::addColumn<QString>("phase");
        QTest::newRow("reload") << QStringLiteral("reload");
        QTest::newRow("apply-preflight") << QStringLiteral("preflight");
        QTest::newRow("apply-readback") << QStringLiteral("readback");
        QTest::newRow("diagnostics-preflight") << QStringLiteral("diagnostics-preflight");
        QTest::newRow("diagnostics-readback") << QStringLiteral("diagnostics-readback");
    }
    void readTimeoutIgnoresLateReply() {
        QFETCH(QString, phase);
        SettingsController controller(*client_);
        load(controller);
        const bool readback = phase.endsWith(QStringLiteral("readback"));
        mock_->holdReads = !readback;
        mock_->holdReadback = readback;
        if (phase == QStringLiteral("reload")) controller.reload();
        else if (phase.startsWith(QStringLiteral("diagnostics-"))) controller.applyDiagnostics(QStringLiteral("False"));
        else controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.result(), readback ? QStringLiteral("unconfirmed") : QStringLiteral("error"));
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QCOMPARE(mock_->writes, readback ? 1 : 0);
        mock_->holdReadback = false;
        mock_->releaseReads();
        QTest::qWait(50);
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QCOMPARE(mock_->writes, readback ? 1 : 0);
        load(controller); // A new explicit request can recover after timeout.
    }

    void optionalGroupValidation_data() {
        QTest::addColumn<QString>("defect");
        for (const char* defect : {"legacy", "valid", "missing-check-runtime", "missing-modern-runtime",
                "missing-check-schema", "missing-modern-schema", "runtime-only", "schema-only",
                "check-type", "modern-type", "check-Bool-type", "modern-Bool-type",
                "duplicate-check", "duplicate-modern", "check-name-case", "modern-name-case",
                "check-schema-case", "modern-schema-case"})
            QTest::newRow(defect) << QString::fromLatin1(defect);
    }
    void optionalGroupValidation() {
        QFETCH(QString, defect);
        enableSpelling();
        if (defect == QStringLiteral("legacy")) { mock_->optionalRuntime.clear(); mock_->optionalSchema.clear(); }
        if (defect == QStringLiteral("runtime-only")) mock_->optionalSchema.clear();
        if (defect == QStringLiteral("schema-only")) mock_->optionalRuntime.clear();
        for (int i = 0; i < 2; ++i) {
            const QString prefix = i == 0 ? QStringLiteral("check") : QStringLiteral("modern");
            const QString key = mock_->optionalSchema.isEmpty() ? QString() : mock_->optionalSchema[i].name;
            if (defect == QStringLiteral("missing-") + prefix + QStringLiteral("-runtime")) mock_->optionalRuntime.remove(key);
            if (defect == prefix + QStringLiteral("-name-case")) {
                const auto value = mock_->optionalRuntime.take(key);
                mock_->optionalRuntime[key.toLower()] = value;
            }
            if (defect == prefix + QStringLiteral("-type")) mock_->optionalSchema[i].type = QStringLiteral("String");
            if (defect == prefix + QStringLiteral("-Bool-type")) mock_->optionalSchema[i].type = QStringLiteral("Bool");
            if (defect == prefix + QStringLiteral("-schema-case")) mock_->optionalSchema[i].name = key.toLower();
        }
        if (defect == QStringLiteral("missing-check-schema")) mock_->optionalSchema.removeAt(0);
        if (defect == QStringLiteral("missing-modern-schema")) mock_->optionalSchema.removeAt(1);
        if (defect == QStringLiteral("duplicate-check")) mock_->optionalSchema.append(mock_->optionalSchema[0]);
        if (defect == QStringLiteral("duplicate-modern")) mock_->optionalSchema.append(mock_->optionalSchema[1]);
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(controller.quickTelexSupported());
        const bool supported = defect == QStringLiteral("valid");
        QCOMPARE(controller.spellingSupported(), supported);
        QCOMPARE(controller.runtimeCheckSpelling(), supported ? QStringLiteral("True") : QString());
        QCOMPARE(controller.runtimeModernOrthography(), supported ? QStringLiteral("False") : QString());
        const QVariantMap optionalBefore = mock_->optionalRuntime;
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        controller.applyDiagnostics(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(controller.ready());
        QCOMPARE(controller.spellingSupported(), supported);
        QCOMPARE(mock_->writes, 2);
        QCOMPARE(mock_->optionalRuntime, optionalBefore);
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
        QCOMPARE(controller.runtimeDiagnostics(), QStringLiteral("False"));
        controller.applyQuickTelex(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(controller.runtimeQuickTelex(), QStringLiteral("False"));
        QCOMPARE(controller.spellingSupported(), supported);
        QCOMPARE(mock_->lastPatch, (QVariantMap{{QStringLiteral("QuickTelex"), QStringLiteral("False")}}));
    }

    void malformedOptionalRuntime_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QVariant>("value");
        for (const QString& key : {QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography")}) {
            const QList<QVariant> values{true, false, QString(), QStringLiteral("true"),
                QStringLiteral("false"), QStringLiteral(" True"), QStringLiteral("False "), 1};
            for (int i = 0; i < values.size(); ++i)
                QTest::newRow(qPrintable(key + QString::number(i))) << key << values[i];
        }
    }
    void malformedOptionalRuntime() {
        QFETCH(QString, key);
        QFETCH(QVariant, value);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(controller.spellingSupported());
        mock_->optionalRuntime[key] = value;
        load(controller); // Losing optional support clears both previously loaded controls.
        QVERIFY(!controller.spellingSupported());
        QVERIFY(controller.runtimeCheckSpelling().isEmpty());
        QVERIFY(controller.runtimeModernOrthography().isEmpty());
        controller.applyDiagnostics(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(controller.ready());
        QCOMPARE(mock_->writes, 1);
        QVERIFY(controller.quickTelexSupported());
        controller.applyQuickTelex(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(controller.runtimeQuickTelex(), QStringLiteral("False"));
        QVERIFY(!controller.spellingSupported());
        QCOMPARE(mock_->writes, 2);
    }

    void quickTelexValidation_data() {
        QTest::addColumn<QString>("defect");
        for (const char* defect : {"valid", "legacy-alpha10", "missing-schema", "missing-runtime",
                "wrong-type", "Bool-type", "duplicate", "duplicate-inputconfig", "wrong-group",
                "other-group-ignored", "schema-case", "runtime-case", "wire-true", "wire-false",
                "lowercase", "whitespace", "empty", "number", "empty-default", "wrong-default"})
            QTest::newRow(defect) << QString::fromLatin1(defect);
    }
    void quickTelexValidation() {
        QFETCH(QString, defect);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        QVERIFY(controller.quickTelexSupported());
        const QString key = QStringLiteral("QuickTelex");
        if (defect == QStringLiteral("legacy-alpha10") || defect == QStringLiteral("missing-schema"))
            mock_->optionalSchema.removeLast();
        if (defect == QStringLiteral("legacy-alpha10") || defect == QStringLiteral("missing-runtime"))
            mock_->optionalRuntime.remove(key); // Schema default must never supply runtime state.
        if (defect == QStringLiteral("wrong-type")) mock_->optionalSchema.last().type = QStringLiteral("String");
        if (defect == QStringLiteral("Bool-type")) mock_->optionalSchema.last().type = QStringLiteral("Bool");
        if (defect == QStringLiteral("duplicate")) mock_->optionalSchema.append(mock_->optionalSchema.last());
        if (defect == QStringLiteral("duplicate-inputconfig"))
            mock_->extraSchema.append({QStringLiteral("InputConfig"), {mock_->optionalSchema.last()}});
        if (defect == QStringLiteral("wrong-group") || defect == QStringLiteral("other-group-ignored")) {
            mock_->extraSchema.append({QStringLiteral("OtherConfig"), {mock_->optionalSchema.last()}});
            if (defect == QStringLiteral("wrong-group")) mock_->optionalSchema.removeLast();
        }
        if (defect == QStringLiteral("schema-case")) mock_->optionalSchema.last().name = key.toLower();
        if (defect == QStringLiteral("runtime-case")) mock_->optionalRuntime[key.toLower()] = mock_->optionalRuntime.take(key);
        if (defect == QStringLiteral("wire-true")) mock_->optionalRuntime[key] = true;
        if (defect == QStringLiteral("wire-false")) mock_->optionalRuntime[key] = false;
        if (defect == QStringLiteral("lowercase")) mock_->optionalRuntime[key] = QStringLiteral("true");
        if (defect == QStringLiteral("whitespace")) mock_->optionalRuntime[key] = QStringLiteral("False ");
        if (defect == QStringLiteral("empty")) mock_->optionalRuntime[key] = QString();
        if (defect == QStringLiteral("number")) mock_->optionalRuntime[key] = 1;
        if (defect == QStringLiteral("empty-default")) mock_->optionalSchema.last().defaultValue = QDBusVariant(QString());
        if (defect == QStringLiteral("wrong-default")) mock_->optionalSchema.last().defaultValue = QDBusVariant(42);
        load(controller);
        const bool supported = defect == QStringLiteral("valid") || defect == QStringLiteral("empty-default") ||
            defect == QStringLiteral("wrong-default") || defect == QStringLiteral("other-group-ignored");
        QCOMPARE(controller.quickTelexSupported(), supported);
        QCOMPARE(controller.runtimeQuickTelex(), supported ? QStringLiteral("True") : QString());
        QVERIFY(controller.spellingSupported());
        const auto quickBefore = mock_->optionalRuntime.value(key);
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        controller.applyDiagnostics(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        controller.applySpelling(QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        controller.applyModernOrthography(QStringLiteral("True"));
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(mock_->writes, 4);
        QCOMPARE(mock_->optionalRuntime.value(key), quickBefore);
        QCOMPARE(controller.quickTelexSupported(), supported);
        QVERIFY(controller.spellingSupported());
        if (!supported) {
            const int reads = mock_->reads;
            controller.applyQuickTelex(QStringLiteral("False"));
            QCOMPARE(controller.result(), QStringLiteral("unsupported"));
            QCOMPARE(mock_->reads, reads);
            QCOMPARE(mock_->writes, 4);
        }
    }

    void unsupportedOptionalNeverWrites_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<bool>("lostDuringPreflight");
        for (const QString& key : {QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography"), QStringLiteral("QuickTelex")})
            for (bool lost : {false, true})
                QTest::newRow(qPrintable(key + (lost ? "-preflight" : "-legacy"))) << key << lost;
    }
    void unsupportedOptionalNeverWrites() {
        QFETCH(QString, key);
        QFETCH(bool, lostDuringPreflight);
        if (lostDuringPreflight) { enableSpelling(); enableQuickTelex(); }
        SettingsController controller(*client_);
        load(controller);
        if (lostDuringPreflight && key == QStringLiteral("QuickTelex")) mock_->optionalSchema.removeLast();
        else mock_->optionalSchema.clear();
        const int before = mock_->reads;
        applyField(controller, key, QStringLiteral("False"));
        QTRY_COMPARE(controller.result(), QStringLiteral("unsupported"));
        verifyCleared(controller);
        QCOMPARE(mock_->writes, 0);
        if (!lostDuringPreflight) QCOMPARE(mock_->reads, before);
        else QVERIFY(mock_->reads > before);
    }

    void optionalToggle_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("target");
        for (const QString& key : {QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography"), QStringLiteral("QuickTelex")})
            for (const QString& target : {QStringLiteral("True"), QStringLiteral("False")})
                QTest::newRow(qPrintable(key + target)) << key << target;
    }
    void optionalToggle() {
        QFETCH(QString, key);
        QFETCH(QString, target);
        enableSpelling();
        enableQuickTelex();
        mock_->optionalRuntime[key] = target == QStringLiteral("True") ? QStringLiteral("False") : QStringLiteral("True");
        SettingsController controller(*client_);
        load(controller);
        QVariantMap expected = mock_->optionalRuntime;
        expected[key] = target;
        const int before = mock_->reads;
        applyField(controller, key, target);
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QVERIFY(controller.ready());
        QVERIFY(controller.spellingSupported());
        QVERIFY(controller.quickTelexSupported());
        QCOMPARE(mock_->reads, before + 2);
        QCOMPARE(mock_->writes, 1);
        QCOMPARE(mock_->lastPatch, (QVariantMap{{key, target}}));
        QCOMPARE(mock_->optionalRuntime, expected);
        QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
        QCOMPARE(mock_->diagnostics, QStringLiteral("True"));
        QCOMPARE(controller.runtimeQuickTelex(), expected.value(QStringLiteral("QuickTelex")).toString());
        QCOMPARE(controller.runtimeCheckSpelling(), expected.value(QStringLiteral("CheckSpelling")).toString());
        QCOMPARE(controller.runtimeModernOrthography(), expected.value(QStringLiteral("UseModernOrthography")).toString());
        // Same value still preflights, then confirms without a second write.
        applyField(controller, key, target);
        QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
        QCOMPARE(mock_->reads, before + 3);
        QCOMPARE(mock_->writes, 1);
    }

    void invalidOptionalTarget_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("target");
        for (const QString& key : {QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography"), QStringLiteral("QuickTelex")})
            for (const QString& target : {QString(), QStringLiteral("true"), QStringLiteral("false"),
                    QStringLiteral(" True "), QStringLiteral("1"), QStringLiteral("Direct")})
                QTest::newRow(qPrintable(key + ":" + target)) << key << target;
    }
    void invalidOptionalTarget() {
        QFETCH(QString, key);
        QFETCH(QString, target);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        applyField(controller, key, target);
        QTRY_COMPARE(controller.result(), QStringLiteral("invalid"));
        verifyCleared(controller);
        QCOMPARE(mock_->reads, before);
        QCOMPARE(mock_->writes, 0);
    }

    void optionalFieldConflicts_data() {
        QTest::addColumn<QString>("selected");
        QTest::addColumn<QString>("changed");
        QTest::addColumn<bool>("duringReadback");
        const QStringList fields{QStringLiteral("InputStyle"), QStringLiteral("DirectDiagnostics"),
            QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography"), QStringLiteral("QuickTelex")};
        for (const QString& selected : fields)
            for (const QString& changed : fields)
                for (bool readback : {false, true})
                    QTest::newRow(qPrintable(selected + ":" + changed + (readback ? ":readback" : ":preflight")))
                        << selected << changed << readback;
    }
    void optionalFieldConflicts() {
        QFETCH(QString, selected);
        QFETCH(QString, changed);
        QFETCH(bool, duringReadback);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        const auto otherValue = [](const QString& key) {
            if (key == QStringLiteral("InputStyle")) return QStringLiteral("Direct");
            if (key == QStringLiteral("UseModernOrthography")) return QStringLiteral("True");
            return QStringLiteral("False");
        };
        const QString target = otherValue(selected);
        if (duringReadback) {
            // Selected-field readback mismatch must reject the ACK; unrelated changes are accepted.
            QString value = otherValue(changed);
            if (changed == selected) value = changed == QStringLiteral("InputStyle") ? QStringLiteral("Preedit") :
                (value == QStringLiteral("True") ? QStringLiteral("False") : QStringLiteral("True"));
            mock_->changesAfterWrite[changed] = value;
        } else {
            if (changed == QStringLiteral("InputStyle")) mock_->runtime = otherValue(changed);
            else if (changed == QStringLiteral("DirectDiagnostics")) mock_->diagnostics = otherValue(changed);
            else mock_->optionalRuntime[changed] = otherValue(changed);
        }
        applyField(controller, selected, target);
        const bool conflict = selected == changed;
        QTRY_COMPARE(controller.result(), conflict ?
            (duringReadback ? QStringLiteral("unconfirmed") : QStringLiteral("conflict")) : QStringLiteral("confirmed"));
        QCOMPARE(mock_->writes, conflict && !duringReadback ? 0 : 1);
        if (conflict) verifyCleared(controller);
        else {
            QVERIFY(controller.ready());
            QVERIFY(controller.spellingSupported());
            QVERIFY(controller.quickTelexSupported());
            QCOMPARE(mock_->lastPatch, (QVariantMap{{selected, target}}));
            QCOMPARE(controller.runtimeStyle(), mock_->runtime);
            QCOMPARE(controller.runtimeDiagnostics(), mock_->diagnostics);
            QCOMPARE(controller.runtimeQuickTelex(), mock_->optionalRuntime.value(QStringLiteral("QuickTelex")).toString());
            QCOMPARE(controller.runtimeCheckSpelling(), mock_->optionalRuntime.value(QStringLiteral("CheckSpelling")).toString());
            QCOMPARE(controller.runtimeModernOrthography(), mock_->optionalRuntime.value(QStringLiteral("UseModernOrthography")).toString());
        }
    }

    void optionalApplyFailure_data() {
        QTest::addColumn<QString>("key");
        QTest::addColumn<QString>("failure");
        for (const QString& key : {QStringLiteral("CheckSpelling"), QStringLiteral("UseModernOrthography"), QStringLiteral("QuickTelex")})
            for (const QString& failure : {QStringLiteral("preflight-error"), QStringLiteral("readback-error"),
                    QStringLiteral("write-error"), QStringLiteral("ignored"), QStringLiteral("preflight-timeout"),
                    QStringLiteral("readback-timeout"), QStringLiteral("readback-support-lost"),
                    QStringLiteral("write-timeout"), QStringLiteral("readback-malformed")})
                QTest::newRow(qPrintable(key + ":" + failure)) << key << failure;
    }
    void optionalApplyFailure() {
        QFETCH(QString, key);
        QFETCH(QString, failure);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        mock_->failReads = failure == QStringLiteral("preflight-error");
        mock_->failReadsAfterWrite = failure == QStringLiteral("readback-error");
        mock_->holdWrites = failure == QStringLiteral("write-timeout");
        mock_->failWrite = failure == QStringLiteral("write-error");
        mock_->ignoreApply = failure == QStringLiteral("ignored");
        mock_->holdReads = failure == QStringLiteral("preflight-timeout");
        mock_->holdReadback = failure == QStringLiteral("readback-timeout") || failure == QStringLiteral("readback-support-lost") ||
            failure == QStringLiteral("readback-malformed");
        const QString target = key == QStringLiteral("UseModernOrthography") ? QStringLiteral("True") : QStringLiteral("False");
        applyField(controller, key, target);
        if (failure == QStringLiteral("readback-support-lost") || failure == QStringLiteral("readback-malformed")) {
            QTRY_COMPARE(mock_->pendingReads.size(), 1);
            if (failure == QStringLiteral("readback-support-lost")) {
                if (key == QStringLiteral("QuickTelex")) mock_->optionalSchema.removeLast();
                else mock_->optionalSchema.clear();
            }
            else mock_->optionalRuntime[key] = true; // Wire Boolean is not a runtime QString.
            mock_->holdReadback = false;
            mock_->releaseReads();
        }
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.result(), failure.startsWith(QStringLiteral("preflight-")) ? QStringLiteral("error") : QStringLiteral("unconfirmed"));
        verifyCleared(controller);
        const int writes = failure.startsWith(QStringLiteral("preflight-")) ? 0 : 1;
        QCOMPARE(mock_->writes, writes);
        const int reads = mock_->reads;
        mock_->holdReadback = false;
        for (const auto& message : mock_->pendingWrites) server_->send(message.createReply());
        mock_->pendingWrites.clear();
        mock_->releaseReads(); // Late success cannot re-enable the controls or trigger retries.
        QTest::qWait(50);
        verifyCleared(controller);
        QCOMPARE(mock_->reads, reads);
        QCOMPARE(mock_->writes, writes);
    }

    void quickTelexBusyAndOwnerChange_data() {
        QTest::addColumn<bool>("readback");
        QTest::addColumn<bool>("loseOwner");
        for (bool readback : {false, true})
            for (bool loseOwner : {false, true})
                QTest::newRow(qPrintable(QString::number(readback) + ":" + QString::number(loseOwner)))
                    << readback << loseOwner;
    }
    void quickTelexBusyAndOwnerChange() {
        QFETCH(bool, readback);
        QFETCH(bool, loseOwner);
        enableSpelling();
        enableQuickTelex();
        SettingsController controller(*client_);
        load(controller);
        mock_->holdReads = !readback;
        mock_->holdReadback = readback;
        controller.applyQuickTelex(QStringLiteral("False"));
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        const int reads = mock_->reads, writes = mock_->writes;
        controller.applyQuickTelex(QStringLiteral("True"));
        controller.applyQuickTelex(QStringLiteral("invalid"));
        controller.apply(QStringLiteral("Direct"));
        controller.applyDiagnostics(QStringLiteral("False"));
        controller.applySpelling(QStringLiteral("False"));
        controller.applyModernOrthography(QStringLiteral("True"));
        controller.reload();
        QTest::qWait(30);
        QCOMPARE(controller.result(), QStringLiteral("applying"));
        QCOMPARE(mock_->reads, reads);
        QCOMPARE(mock_->writes, writes);
        if (loseOwner) {
            QVERIFY(server_->unregisterService(service));
            QTRY_VERIFY(!controller.busy());
            QCOMPARE(controller.result(), readback ? QStringLiteral("unconfirmed") : QStringLiteral("unavailable"));
        }
        mock_->holdReadback = false;
        mock_->releaseReads();
        if (loseOwner) {
            QTest::qWait(50); // Old-owner replies cannot resume the pipeline.
            verifyCleared(controller);
            QCOMPARE(mock_->reads, reads);
            QCOMPARE(mock_->writes, writes);
        } else {
            QTRY_COMPARE(controller.result(), QStringLiteral("confirmed"));
            QCOMPARE(controller.runtimeQuickTelex(), QStringLiteral("False"));
            QCOMPARE(mock_->writes, 1);
            QTest::qWait(30);
            QCOMPARE(mock_->reads, readback ? reads : reads + 1);
            QCOMPARE(mock_->writes, 1);
        }
    }

    void missingOwnerNeverWrites() {
        QVERIFY(server_->unregisterService(service));
        SettingsController controller(*client_);
        controller.reload();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QVERIFY(controller.result() == QStringLiteral("offline") ||
                controller.result() == QStringLiteral("unavailable"));
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
    }

    void disappearingOwnerInvalidatesSnapshot() {
        SettingsController controller(*client_);
        load(controller);
        QSignalSpy changed(&controller, &SettingsController::changed);
        QVERIFY(server_->unregisterService(service));
        QTRY_VERIFY(!controller.ready());
        QTRY_VERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(!controller.busy());
        QVERIFY(!changed.isEmpty());
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
    }

    void disappearingOwnerDuringApplyRejectsLateReply() {
        SettingsController controller(*client_);
        load(controller);
        mock_->holdReads = true;
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        QVERIFY(server_->unregisterService(service));
        QTRY_VERIFY(!controller.ready());
        QTRY_VERIFY(!controller.busy());
        mock_->releaseReads(); // A stale success from the old owner must not write.
        QTest::qWait(50);
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QVERIFY(controller.runtimeDiagnostics().isEmpty());
        QCOMPARE(mock_->writes, 0);
        QVERIFY(controller.result() != QStringLiteral("confirmed"));
    }
};

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("OPENKEY_SETTINGS_TEST_PRIVATE_BUS")) {
        // Re-exec before creating any QDBusConnection, even if launched manually.
        QProcess runner;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("OPENKEY_SETTINGS_TEST_PRIVATE_BUS"), QStringLiteral("1"));
        environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_BUS_TYPE"));
        runner.setProcessEnvironment(environment);
        runner.setProcessChannelMode(QProcess::ForwardedChannels);
        runner.start(QStringLiteral("dbus-run-session"),
                     QStringList{QStringLiteral("--"), QCoreApplication::applicationFilePath()}
                         + QCoreApplication::arguments().mid(1));
        if (!runner.waitForStarted()) {
            qCritical("Cannot start private dbus-run-session: %s", qPrintable(runner.errorString()));
            return 1;
        }
        runner.waitForFinished(-1);
        return runner.exitStatus() == QProcess::NormalExit ? runner.exitCode() : 1;
    }
    LinuxSettingsControllerTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "LinuxSettingsControllerTests.moc"
