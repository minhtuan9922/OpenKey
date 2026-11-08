#include "InputMethodController.h"
#include <QCoreApplication>
#include <QDBusArgument>
#include <QDBusError>
#include <QDBusMetaType>
#include <QDBusVirtualObject>
#include <QProcess>
#include <QDBusPendingCallWatcher>
#include <QTemporaryDir>
#include <QFile>
#include "SettingsOverview.h"
#include <QtTest>
#include <memory>
struct Method { QString name, layout; };
using Methods = QList<Method>;
Q_DECLARE_METATYPE(Method)
Q_DECLARE_METATYPE(Methods)
QDBusArgument& operator<<(QDBusArgument& a, const Method& v) { a.beginStructure(); a << v.name << v.layout; a.endStructure(); return a; }
const QDBusArgument& operator>>(const QDBusArgument& a, Method& v) { a.beginStructure(); a >> v.name >> v.layout; a.endStructure(); return a; }
class Mock : public QDBusVirtualObject {
public:
    QString group = "Default", current = "openkey-telex";
    Methods methods{{"keyboard-us", ""}, {"openkey-telex", ""}, {"openkey-vni", ""}, {"openkey-simple-telex1", ""}, {"openkey-simple-telex2", ""}};
    int state = 2, writes = 0, reads = 0;
    bool ignore = false, hold = false, malformed = false, loseOwner = false;
    QStringList unexpected;
    QString registeredOwner;
    QString introspect(const QString&) const override { return {}; }
    bool handleMessage(const QDBusMessage& m, const QDBusConnection& bus) override {
        const auto member = m.member();
        if (member == "RegisterStatusNotifierItem") { registeredOwner = m.service(); bus.send(m.createReply()); return true; }
        if (member == "SetCurrentIM" && m.signature() == "s") {
            ++writes;
            if (!ignore) { current = m.arguments()[0].toString(); state = 2; }
            if (loseOwner) { auto connection = bus; connection.unregisterService("org.fcitx.Fcitx5"); }
            if (!hold) bus.send(m.createReply());
            return true;
        }
        ++reads;
        if (member == "CurrentInputMethodGroup") bus.send(m.createReply(QVariantList{group}));
        else if (member == "InputMethodGroupInfo" && m.arguments() == QVariantList{group}) {
            if (malformed) bus.send(m.createReply(QVariantList{QString("bad")}));
            else bus.send(m.createReply(QVariantList{QString("us"), QVariant::fromValue(methods)}));
        } else if (member == "CurrentInputMethod") bus.send(m.createReply(QVariantList{current}));
        else if (member == "State") bus.send(m.createReply(QVariantList{state}));
        else { unexpected.append(member); bus.send(m.createErrorReply(QDBusError::UnknownMethod, "Unexpected call")); }
        return true;
    }
};
class ControllerTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QDBusConnection> server, client;
    std::unique_ptr<Mock> mock;
    void load(InputMethodController& model) { model.reload(); QTRY_COMPARE(model.result(), QString("loaded")); QVERIFY(model.ready()); }
private slots:
    void initTestCase() { qDBusRegisterMetaType<Method>(); qDBusRegisterMetaType<Methods>(); }
    void init() {
        const auto address = qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS");
        server = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, "im-server"));
        client = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(address, "im-client"));
        QVERIFY(server->isConnected() && client->isConnected());
        mock = std::make_unique<Mock>();
        QVERIFY(server->registerVirtualObject("/controller", mock.get()));
        QVERIFY(server->registerService("org.fcitx.Fcitx5"));
    }
    void cleanup() {
        QVERIFY(mock->unexpected.isEmpty());
        server->unregisterService("org.fcitx.Fcitx5"); server->unregisterObject("/controller");
        mock.reset(); server.reset(); client.reset();
        QDBusConnection::disconnectFromBus("im-server"); QDBusConnection::disconnectFromBus("im-client");
    }
#ifdef OPENKEY_TEST_TRAY_BINARY
    void uiServiceTraySynchronization() {
        QTemporaryDir runtime;
        QVERIFY(runtime.isValid());
        QVERIFY(QFile::setPermissions(runtime.path(), QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner));
        QVERIFY(server->registerVirtualObject("/StatusNotifierWatcher", mock.get()));
        QVERIFY(server->registerService("org.kde.StatusNotifierWatcher"));
        struct ServiceProcess : QProcess { ~ServiceProcess() { if (state() != NotRunning) { terminate(); if (!waitForFinished(1000)) { kill(); waitForFinished(); } } } } process;
        auto env = QProcessEnvironment::systemEnvironment();
        env.insert("XDG_RUNTIME_DIR", runtime.path()); env.insert("XDG_SESSION_TYPE", "wayland");
        process.setProcessEnvironment(env);
        process.start(OPENKEY_TEST_TRAY_BINARY, {"--run-tray"});
        QVERIFY(process.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(!mock->registeredOwner.isEmpty(), 3000);
        InputMethodController model(*client); load(model);
        SettingsOverview overview(runtime.path(), runtime.filePath("absent-config")); overview.start();
        const QStringList methods{"openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2", "keyboard-us", "openkey-telex"};
        const QStringList statuses{"vni", "simple-telex1", "simple-telex2", "english", "telex"};
        const QList<int> ids{911, 912, 913, 920, 910};
        for (int i = 0; i < methods.size(); ++i) {
            model.select(methods[i]); QTRY_COMPARE(model.result(), QString("confirmed"));
            QTRY_COMPARE_WITH_TIMEOUT(overview.inputStatus(), statuses[i], 3000);
            auto message = QDBusMessage::createMethodCall(mock->registeredOwner, "/Menu", "com.canonical.dbusmenu", "GetLayout");
            message.setArguments({ids[i], 0, QStringList{"toggle-state"}}); message.setAutoStartService(false);
            QDBusPendingCallWatcher reply(client->asyncCall(message, 1000));
            QTRY_VERIFY(reply.isFinished());
            const auto response = reply.reply();
            QCOMPARE(response.signature(), "u(ia{sv}av)");
            const auto node = response.arguments()[1].value<QDBusArgument>();
            int id = 0; QVariantMap properties;
            node.beginStructure(); node >> id >> properties;
            QCOMPARE(id, ids[i]); QCOMPARE(properties.value("toggle-state").toInt(), 1);
        }
        QCOMPARE(mock->writes, 5);
        server->unregisterService("org.kde.StatusNotifierWatcher"); server->unregisterObject("/StatusNotifierWatcher");
    }
#endif
    void selectEveryMethod() {
        InputMethodController model(*client);
        load(model);
        QCOMPARE(model.methods().size(), 5);
        QCOMPARE(mock->writes, 0);
        for (const auto& method : {"openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2", "keyboard-us", "openkey-telex"}) {
            model.select(method); QTRY_COMPARE(model.result(), QString("confirmed"));
            QCOMPARE(model.current(), QString(method)); QVERIFY(model.active());
        }
        QCOMPARE(mock->writes, 5);
        const auto json = model.backupJson();
        QCOMPARE(model.inspectBackup(json).value("method").toString(), "openkey-telex");
        QVERIFY(!model.inspectBackup("{}").value("ok").toBool());
        QVERIFY(!model.inspectBackup("{\"format\":\"openkey-input-method\",\"version\":1,\"method\":\"other\"}").value("ok").toBool());
        QCOMPARE(mock->writes, 5);
        model.select("openkey-telex"); QTRY_COMPARE(model.result(), QString("confirmed")); QCOMPARE(mock->writes, 5);
    }
    void guards() {
        InputMethodController model(*client);
        load(model);
        mock->current = "openkey-vni";
        model.select("openkey-simple-telex1"); QTRY_COMPARE(model.result(), QString("conflict")); QCOMPARE(mock->writes, 0);
        load(model);
        mock->group = "Changed";
        model.select("openkey-simple-telex1"); QTRY_COMPARE(model.result(), QString("conflict")); QCOMPARE(mock->writes, 0);
        load(model);
        mock->methods.removeLast();
        model.select("openkey-simple-telex2"); QTRY_COMPARE(model.result(), QString("not-configured")); QCOMPARE(mock->writes, 0);
        load(model);
        model.select("untrusted-method"); QCOMPARE(model.result(), "invalid"); QCOMPARE(mock->writes, 0);
    }
    void inactiveActivation() {
        InputMethodController model(*client);
        mock->state = 1; load(model); QVERIFY(!model.active());
        model.select("openkey-telex"); QTRY_COMPARE(model.result(), QString("confirmed")); QCOMPARE(mock->writes, 1);
    }
    void ignoredWrite() {
        InputMethodController model(*client); load(model);
        mock->ignore = true;
        model.select("openkey-simple-telex1"); QTRY_COMPARE(model.result(), QString("unconfirmed")); QCOMPARE(mock->writes, 1);
        QVERIFY(!model.ready());
    }
    void lostReplyAndBusy() {
        InputMethodController model(*client); load(model);
        mock->hold = true;
        model.select("openkey-simple-telex1");
        model.select("openkey-vni"); model.reload();
        QTRY_COMPARE_WITH_TIMEOUT(model.result(), QString("unconfirmed"), 4000);
        QCOMPARE(mock->writes, 1);
        mock->hold = false; load(model); QCOMPARE(model.current(), "openkey-simple-telex1");
        model.select("openkey-simple-telex1"); QTRY_COMPARE(model.result(), QString("confirmed")); QCOMPARE(mock->writes, 1);
    }
    void ownerLostAfterWrite() {
        InputMethodController model(*client); load(model);
        mock->loseOwner = true;
        model.select("openkey-simple-telex1"); QTRY_COMPARE(model.result(), QString("unconfirmed")); QCOMPARE(mock->writes, 1);
    }
    void malformedAndMissing() {
        InputMethodController model(*client);
        mock->malformed = true; model.reload(); QTRY_COMPARE(model.result(), QString("error"));
        mock->malformed = false; mock->methods.append(mock->methods.first());
        model.reload(); QTRY_COMPARE(model.result(), QString("error"));
        server->unregisterService("org.fcitx.Fcitx5"); QTest::qWait(30);
        model.reload(); QTRY_COMPARE(model.result(), QString("unavailable")); QCOMPARE(mock->writes, 0);
    }
};
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (!qEnvironmentVariableIsSet("OPENKEY_IM_PRIVATE_BUS")) {
        QProcess process;
        auto env = QProcessEnvironment::systemEnvironment(); env.insert("OPENKEY_IM_PRIVATE_BUS", "1");
        process.setProcessEnvironment(env); process.setProcessChannelMode(QProcess::ForwardedChannels);
        process.start("dbus-run-session", QStringList{"--", QCoreApplication::applicationFilePath()} + app.arguments().mid(1));
        if (!process.waitForStarted() || !process.waitForFinished(30000)) return 1;
        return process.exitCode();
    }
    ControllerTests tests; return QTest::qExec(&tests, argc, argv);
}
#include "LinuxInputMethodControllerTests.moc"
