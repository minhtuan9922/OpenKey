// No desktop bus or configuration files: main re-executes on a fresh private bus.
// CMake may also wrap this executable in dbus-run-session; nesting is harmless.
#include "SettingsController.h"

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
    QString styleType = QStringLiteral("Enum");
    QString diagnosticsType = QStringLiteral("Boolean");
    QVariantMap enumValues{{QStringLiteral("0"), QStringLiteral("Preedit")},
                           {QStringLiteral("1"), QStringLiteral("Direct")}};
    bool includeStyle = true, includeDiagnostics = true;
    bool failReads = false, failReadsAfterWrite = false;
    bool failWrite = false, ignoreApply = false, holdReads = false, holdReadback = false;
    int reads = 0, writes = 0;
    QVariantMap lastPatch;
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
        return {{QStringLiteral("InputConfig"), options}};
    }

    void replyToRead(const QDBusMessage& message) {
        if (failReads || (failReadsAfterWrite && writes > 0)) {
            bus_.send(message.createErrorReply(QDBusError::Failed, QStringLiteral("Test read failure")));
            return;
        }
        const QVariantMap raw{{QStringLiteral("InputStyle"), runtime},
                              {QStringLiteral("DirectDiagnostics"), diagnostics}};
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
        if (lastPatch.size() != 1 || style.metaType() != QMetaType::fromType<QString>() ||
            (style.toString() != QStringLiteral("Preedit") && style.toString() != QStringLiteral("Direct"))) {
            violations.append(QStringLiteral("SetConfig must contain only an InputStyle string patch"));
            bus_.send(message.createErrorReply(QDBusError::InvalidArgs, violations.last()));
        } else if (failWrite) {
            bus_.send(message.createErrorReply(QDBusError::Failed, QStringLiteral("Test write failure")));
        } else {
            if (!ignoreApply) runtime = style.toString();
            bus_.send(message.createReply()); // Empty ACK is not proof of application.
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
    }

private slots:
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
        QTest::newRow("preedit") << QStringLiteral("Preedit");
        QTest::newRow("direct") << QStringLiteral("Direct");
    }
    void loadRuntime() {
        QFETCH(QString, style);
        mock_->runtime = style;
        SettingsController controller(*client_);
        QSignalSpy changed(&controller, &SettingsController::changed);
        QVERIFY(changed.isValid());
        load(controller);
        QVERIFY(controller.ready());
        QVERIFY(!changed.isEmpty());
        QVERIFY(mock_->reads >= 1);
        QCOMPARE(mock_->writes, 0);
    }

    void reloadNeverWrites() {
        SettingsController controller(*client_);
        load(controller);
        const int before = mock_->reads;
        mock_->runtime = QStringLiteral("Direct");
        controller.reload();
        QTRY_VERIFY(mock_->reads > before);
        QTRY_VERIFY(!controller.busy());
        QCOMPARE(controller.result(), QStringLiteral("loaded"));
        QCOMPARE(controller.runtimeStyle(), QStringLiteral("Direct"));
        QVERIFY(controller.ready());
        QCOMPARE(mock_->writes, 0);
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
        QCOMPARE(mock_->writes, 0);
        QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
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

    void unsuccessfulApply_data() {
        QTest::addColumn<bool>("readError");
        QTest::addColumn<bool>("writeError");
        QTest::addColumn<bool>("ignoreApply");
        QTest::newRow("readback-error") << true << false << false;
        QTest::newRow("write-error") << false << true << false;
        QTest::newRow("ack-without-change") << false << false << true;
    }
    void unsuccessfulApply() {
        QFETCH(bool, readError);
        QFETCH(bool, writeError);
        QFETCH(bool, ignoreApply);
        SettingsController controller(*client_);
        load(controller);
        mock_->failReadsAfterWrite = readError;
        mock_->failWrite = writeError;
        mock_->ignoreApply = ignoreApply;
        const int before = mock_->reads;
        controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(controller.result(), QStringLiteral("unconfirmed"));
        QVERIFY(!controller.busy());
        QCOMPARE(mock_->writes, 1);
        if (!writeError) QVERIFY(mock_->reads >= before + 2);
        if (readError) QVERIFY(!controller.ready());
        if (ignoreApply || writeError) QCOMPARE(mock_->runtime, QStringLiteral("Preedit"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 1); // No automatic write retry.
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
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
        mock_->failReads = false;
        load(controller);
    }

    void invalidSchemaDisablesReady_data() {
        QTest::addColumn<QString>("defect");
        for (const char* defect : {"missing-style", "missing-diagnostics", "style-type",
                                   "diagnostics-type", "enum-missing", "enum-value", "enum-extra"})
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
        controller.apply(QStringLiteral("Direct"));
        QTest::qWait(30);
        QCOMPARE(mock_->writes, 0);
    }

    void readTimeoutIgnoresLateReply_data() {
        QTest::addColumn<QString>("phase");
        QTest::newRow("reload") << QStringLiteral("reload");
        QTest::newRow("apply-preflight") << QStringLiteral("preflight");
        QTest::newRow("apply-readback") << QStringLiteral("readback");
    }
    void readTimeoutIgnoresLateReply() {
        QFETCH(QString, phase);
        SettingsController controller(*client_);
        load(controller);
        const bool readback = phase == QStringLiteral("readback");
        mock_->holdReads = !readback;
        mock_->holdReadback = readback;
        if (phase == QStringLiteral("reload")) controller.reload();
        else controller.apply(QStringLiteral("Direct"));
        QTRY_COMPARE(mock_->pendingReads.size(), 1);
        QVERIFY(controller.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!controller.busy(), 5000);
        QCOMPARE(controller.result(), readback ? QStringLiteral("unconfirmed") : QStringLiteral("error"));
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QCOMPARE(mock_->writes, readback ? 1 : 0);
        mock_->holdReadback = false;
        mock_->releaseReads();
        QTest::qWait(50);
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
        QCOMPARE(mock_->writes, readback ? 1 : 0);
        load(controller); // A new explicit request can recover after timeout.
    }

    void missingOwnerNeverWrites() {
        QVERIFY(server_->unregisterService(service));
        SettingsController controller(*client_);
        controller.reload();
        QTRY_VERIFY(!controller.busy());
        QVERIFY(!controller.ready());
        QVERIFY(controller.runtimeStyle().isEmpty());
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
