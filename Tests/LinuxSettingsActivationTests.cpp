// Always re-exec on a private bus before constructing the GUI or a bus connection.
#include "SettingsActivation.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusServiceWatcher>
#include <QDBusVirtualObject>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QProcessEnvironment>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <csignal>
#include <memory>
#include <vector>
#include <sys/types.h>

#ifndef OPENKEY_SETTINGS_BINARY
#error OPENKEY_SETTINGS_BINARY must name the main settings GUI executable
#endif

namespace {
const QString service = QStringLiteral("org.openkey.Settings");
const QString path = QStringLiteral("/org/openkey/Settings");
const QString interface = QStringLiteral("org.openkey.Settings1");
const QString connectionName = QStringLiteral("openkey-activation-test");
constexpr auto privateBusMarker = "OPENKEY_ACTIVATION_TEST_PRIVATE_BUS";

bool makePrivateDirectory(const QString& path) {
    return QDir().mkpath(path) && QFile::setPermissions(path,
        QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
}

// An unavailable engine is a normal startup condition. Record all calls and
// answer reads with an error so the real controller must remain read-only.
class ControllerProbe : public QDBusVirtualObject {
public:
    int reads = 0;
    int hotkeyReads = 0;
    int inputMethodReads = 0;
    QStringList unexpected;

    QString introspect(const QString&) const override {
        return QStringLiteral(
            "<interface name=\"org.fcitx.Fcitx.Controller1\">"
            "<method name=\"GetConfig\"><arg direction=\"in\" type=\"s\"/>"
            "<arg direction=\"out\" type=\"v\"/>"
            "<arg direction=\"out\" type=\"a(sa(sssva{sv}))\"/></method>"
            "<method name=\"SetConfig\"><arg direction=\"in\" type=\"s\"/>"
            "<arg direction=\"in\" type=\"v\"/></method></interface>");
    }

    bool handleMessage(const QDBusMessage& message, const QDBusConnection& bus) override {
        if (message.interface() != QStringLiteral("org.fcitx.Fcitx.Controller1"))
            return false;
        if (message.member() == QStringLiteral("GetConfig") &&
            message.arguments() == QVariantList{QStringLiteral("fcitx://config/addon/openkey")})
            ++reads;
        else if (message.member() == QStringLiteral("GetConfig") &&
                 message.arguments() == QVariantList{QStringLiteral("fcitx://config/global")})
            ++hotkeyReads;
        else if (message.member() == "CurrentInputMethodGroup" && message.arguments().isEmpty())
            ++inputMethodReads;
        else
            unexpected.append(message.member());
        bus.send(message.createErrorReply(QDBusError::Failed,
                                         QStringLiteral("Test engine unavailable")));
        return true;
    }
};
#ifdef OPENKEY_ACTIVATION_TRAY_BINARY
class WatcherProbe : public QDBusVirtualObject {
public:
    QString owner;
    QString introspect(const QString&) const override { return {}; }
    bool handleMessage(const QDBusMessage& message, const QDBusConnection& bus) override {
        if (message.member() != "RegisterStatusNotifierItem") return false;
        owner = message.service();
        bus.send(message.createReply());
        return true;
    }
};
#endif
}

class LinuxSettingsActivationTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QTemporaryDir> directory_;
    std::unique_ptr<QDBusConnection> bus_;
    std::vector<std::unique_ptr<QProcess>> children_;
    QProcessEnvironment environment_;
    QString activatedOwner_;
    uint activatedPid_ = 0;

    QString owner() const {
        const QDBusReply<QString> reply = bus_->interface()->serviceOwner(service);
        return reply.isValid() ? reply.value() : QString();
    }

    QProcess* launch(const QStringList& arguments = {}) {
        auto child = std::make_unique<QProcess>();
        child->setProcessEnvironment(environment_);
        child->setProcessChannelMode(QProcess::MergedChannels);
        auto* result = child.get();
        children_.push_back(std::move(child));
        result->start(QString::fromUtf8(OPENKEY_SETTINGS_BINARY), arguments);
        return result;
    }

    void verifySuccessfulExit(QProcess* child) {
        // Pump this process's event loop: it may own the Show endpoint.
        QTRY_COMPARE_WITH_TIMEOUT(child->state(), QProcess::NotRunning, 10000);
        const QByteArray output = child->readAll();
        QVERIFY2(child->exitStatus() == QProcess::NormalExit, output.constData());
        QVERIFY2(child->exitCode() == 0, output.constData());
    }

private slots:
    void initTestCase() {
        QVERIFY(qEnvironmentVariableIsSet(privateBusMarker));
        QVERIFY(!qgetenv("DBUS_SESSION_BUS_ADDRESS").isEmpty());
        QVERIFY2(QFileInfo(QString::fromUtf8(OPENKEY_SETTINGS_BINARY)).isExecutable(),
                 "OPENKEY_SETTINGS_BINARY must point to the built main GUI app");
    }

    void init() {
        directory_ = std::make_unique<QTemporaryDir>();
        QVERIFY(directory_->isValid());
        const QString runtime = directory_->filePath(QStringLiteral("runtime"));
        const QString config = directory_->filePath(QStringLiteral("config"));
        QVERIFY(makePrivateDirectory(runtime));
        QVERIFY(makePrivateDirectory(config));
        environment_ = QProcessEnvironment::systemEnvironment();
        environment_.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
        environment_.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        environment_.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment_.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        environment_.insert(QStringLiteral("QT_IM_MODULE"), QStringLiteral("compose"));
        bus_ = std::make_unique<QDBusConnection>(QDBusConnection::connectToBus(
            qEnvironmentVariable("DBUS_SESSION_BUS_ADDRESS"), connectionName));
        QVERIFY(bus_->isConnected());
        QVERIFY(owner().isEmpty());
    }

    void cleanup() {
        for (const auto& child : children_) {
            if (child->state() != QProcess::NotRunning) {
                child->kill(); // Only QProcesses launched by this fixture.
                QVERIFY(child->waitForFinished(5000));
            }
        }
        children_.clear();
        if (activatedPid_ != 0) {
            // The private activation fixture created this process. Check its
            // unique bus identity before signalling, never scan other PIDs.
            const QDBusReply<uint> pid = bus_->interface()->servicePid(activatedOwner_);
            if (pid.isValid() && pid.value() == activatedPid_) {
                QCOMPARE(::kill(static_cast<pid_t>(activatedPid_), SIGKILL), 0);
                QTRY_VERIFY(owner() != activatedOwner_);
            }
            activatedPid_ = 0;
            activatedOwner_.clear();
        }
        bus_.reset();
        QDBusConnection::disconnectFromBus(connectionName);
        directory_.reset();
    }

    void showWithoutWindowEmitsSignal() {
        SettingsActivation activation(*bus_);
        QSignalSpy requested(&activation, &SettingsActivation::showRequested);
        QVERIFY(requested.isValid());
        activation.Show();
        QCOMPARE(requested.count(), 1);
    }

    void showRestoresWindow() {
        QQuickWindow window;
        window.resize(320, 200);
        SettingsActivation activation(*bus_);
        activation.setWindow(&window);
        QSignalSpy requested(&activation, &SettingsActivation::showRequested);
        window.showMinimized();
        QTRY_COMPARE(window.windowState(), Qt::WindowMinimized);
        activation.Show();
        QCOMPARE(requested.count(), 1);
        QTRY_VERIFY(window.isVisible());
        QTRY_VERIFY(window.windowState() != Qt::WindowMinimized);
        window.hide();
        activation.Show();
        QCOMPARE(requested.count(), 2);
        QTRY_VERIFY(window.isVisible());
        activation.setWindow(nullptr);
        activation.Show();
        QCOMPARE(requested.count(), 3);
    }

    void pageNavigationPreservesWindowAndRejectsUnknownPages() {
        SettingsActivation activation(*bus_);
        QSignalSpy requested(&activation, &SettingsActivation::showRequested);
        QVERIFY(activation.ShowPage("macro")); // Queue before the QML window exists.
        QQuickWindow window;
        activation.setWindow(&window);
        QCOMPARE(window.property("pageIndex").toInt(), 5);
        for (const auto& page : SettingsActivation::pages()) {
            window.hide();
            QVERIFY(activation.ShowPage(page));
            QCOMPARE(window.property("pageIndex").toInt(), SettingsActivation::pages().indexOf(page));
            QTRY_VERIFY(window.isVisible());
        }
        window.hide();
        const int count = requested.count();
        for (const auto& page : {QString(), QString("Macro"), QString("../macro"), QString("https://example.com")})
            QVERIFY(!activation.ShowPage(page));
        QCOMPARE(requested.count(), count);
        QVERIFY(!window.isVisible());
        QCOMPARE(window.property("pageIndex").toInt(), 7);
        activation.Show();
        QCOMPARE(window.property("pageIndex").toInt(), 7); // Ordinary Show keeps the user's page.
    }

    void pageNavigationIsExportedOnBus() {
        SettingsActivation activation(*bus_);
        QQuickWindow window;
        activation.setWindow(&window);
        QCOMPARE(activation.claim(), SettingsActivation::Claim::Primary);
        for (const auto& page : {QString("macro"), QString("convert"), QString("about"), QString("invalid")}) {
            auto message = QDBusMessage::createMethodCall(owner(), path, interface, "ShowPage");
            message << page;
            QDBusPendingCallWatcher reply(bus_->asyncCall(message));
            QTRY_VERIFY(reply.isFinished());
            QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
            QCOMPARE(reply.reply().signature(), QString("b"));
            QCOMPARE(reply.reply().arguments().first().toBool(), page != "invalid");
            if (page != "invalid")
                QCOMPARE(window.property("pageIndex").toInt(), SettingsActivation::pages().indexOf(page));
        }
    }

#ifdef OPENKEY_ACTIVATION_TRAY_BINARY
    void trayPageEventsNavigateTheExistingWindow() {
        // Keep the fixture's Unix socket below sockaddr_un's path limit even
        // when the test runner's TMPDIR and application name are long.
        QTemporaryDir runtime(QStringLiteral("/tmp/openkey-page-XXXXXX"));
        QVERIFY(runtime.isValid());
        WatcherProbe watcher;
        QVERIFY(bus_->registerVirtualObject("/StatusNotifierWatcher", &watcher));
        QVERIFY(bus_->registerService("org.kde.StatusNotifierWatcher"));
        SettingsActivation activation(*bus_);
        QQuickWindow window;
        window.setProperty("pageIndex", 0);
        activation.setWindow(&window);
        QCOMPARE(activation.claim(), SettingsActivation::Claim::Primary);
        QSignalSpy requested(&activation, &SettingsActivation::showRequested);
        struct ServiceProcess : QProcess {
            ~ServiceProcess() { if (state() != NotRunning) { kill(); waitForFinished(5000); } }
        } process;
        auto environment = environment_;
        environment.insert("XDG_SESSION_TYPE", "wayland");
        environment.insert("XDG_RUNTIME_DIR", runtime.path());
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(OPENKEY_ACTIVATION_TRAY_BINARY, {"--run-tray"});
        QVERIFY(process.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(!watcher.owner.isEmpty() || process.state() == QProcess::NotRunning, 5000);
        QVERIFY2(!watcher.owner.isEmpty(), process.readAll().constData());
        for (const auto& command : {qMakePair(990, 5), qMakePair(980, 6), qMakePair(1010, 7)}) {
            auto event = QDBusMessage::createMethodCall(watcher.owner, "/Menu", "com.canonical.dbusmenu", "Event");
            event.setArguments({command.first, "hovered", QVariant::fromValue(QDBusVariant(0)), uint(0)});
            QDBusPendingCallWatcher hover(bus_->asyncCall(event));
            QTRY_VERIFY(hover.isFinished());
            QVERIFY(!hover.isError());
            QCOMPARE(requested.count(), command.second - 5);
            window.hide();
            event.setArguments({command.first, "clicked", QVariant::fromValue(QDBusVariant(0)), uint(0)});
            QDBusPendingCallWatcher clicked(bus_->asyncCall(event));
            QTRY_VERIFY(clicked.isFinished());
            QVERIFY(!clicked.isError());
            QTRY_COMPARE(window.property("pageIndex").toInt(), command.second);
            QTRY_COMPARE(requested.count(), command.second - 4);
            QVERIFY(window.isVisible());
        }
        QCOMPARE(owner(), bus_->baseService()); // No second settings window or owner.
        bus_->unregisterService("org.kde.StatusNotifierWatcher");
        bus_->unregisterObject("/StatusNotifierWatcher");
    }
#endif

    void primaryExportsShowAndReleasesClaim() {
        {
            SettingsActivation activation(*bus_);
            QSignalSpy requested(&activation, &SettingsActivation::showRequested);
            QCOMPARE(activation.claim(), SettingsActivation::Claim::Primary);
            QCOMPARE(owner(), bus_->baseService());
            auto message = QDBusMessage::createMethodCall(owner(), path, interface,
                                                          QStringLiteral("Show"));
            message.setAutoStartService(false);
            QDBusPendingCallWatcher reply(bus_->asyncCall(message));
            QTRY_VERIFY(reply.isFinished());
            QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
            QTRY_COMPARE(requested.count(), 1);
        }
        QTRY_VERIFY(owner().isEmpty());
        SettingsActivation replacement(*bus_);
        QCOMPARE(replacement.claim(), SettingsActivation::Claim::Primary);
    }

    void sameConnectionCollisionPreservesOriginal() {
        SettingsActivation primary(*bus_);
        QSignalSpy requested(&primary, &SettingsActivation::showRequested);
        QCOMPARE(primary.claim(), SettingsActivation::Claim::Primary);
        {
            SettingsActivation collision(*bus_);
            QCOMPARE(collision.claim(), SettingsActivation::Claim::Failed);
            QCOMPARE(owner(), bus_->baseService());
        }
        QCOMPARE(owner(), bus_->baseService());
        auto message = QDBusMessage::createMethodCall(owner(), path, interface,
                                                      QStringLiteral("Show"));
        message.setAutoStartService(false);
        QDBusPendingCallWatcher reply(bus_->asyncCall(message));
        QTRY_VERIFY(reply.isFinished());
        QVERIFY2(!reply.isError(), qPrintable(reply.error().message()));
        QTRY_COMPARE(requested.count(), 1);
    }

    void missingBusFails() {
        // Explicit invalid address: never ask Qt to discover the desktop bus.
        const QString name = QStringLiteral("openkey-activation-missing-bus");
        {
            const auto missing = QDBusConnection::connectToBus(
                QStringLiteral("unix:path=") + directory_->filePath(QStringLiteral("absent-bus")), name);
            QVERIFY(!missing.isConnected());
            SettingsActivation activation(missing);
            QCOMPARE(activation.claim(), SettingsActivation::Claim::Failed);
        }
        QDBusConnection::disconnectFromBus(name);
        QVERIFY(owner().isEmpty());
    }

    void disconnectedBusFails() {
        QDBusConnection disconnected(QStringLiteral("openkey-activation-no-connection"));
        QVERIFY(!disconnected.isConnected());
        SettingsActivation activation(disconnected);
        QCOMPARE(activation.claim(), SettingsActivation::Claim::Failed);
        QVERIFY(owner().isEmpty());
    }

    void normalBinaryIsSingletonAndStartsReadOnlyController() {
        ControllerProbe controller;
        QVERIFY(bus_->registerVirtualObject(QStringLiteral("/controller"), &controller));
        QVERIFY(bus_->registerService(QStringLiteral("org.fcitx.Fcitx5")));
        auto* first = launch();
        QVERIFY2(first->waitForStarted(), qPrintable(first->errorString()));
        QTRY_VERIFY_WITH_TIMEOUT(!owner().isEmpty(), 10000);
        const QString originalOwner = owner();
        const QDBusReply<uint> originalPid = bus_->interface()->servicePid(service);
        QVERIFY(originalPid.isValid());
        QCOMPARE(qint64(originalPid.value()), first->processId());
        QTRY_VERIFY_WITH_TIMEOUT(controller.reads > 0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.hotkeyReads > 0, 5000);
        QTRY_VERIFY_WITH_TIMEOUT(controller.inputMethodReads > 0, 5000);

        // Also exercise the Forwarded enum against an owner in another process.
        {
            SettingsActivation forwarded(*bus_);
            QCOMPARE(forwarded.claim(), SettingsActivation::Claim::Forwarded);
        }
        QCOMPARE(owner(), originalOwner);
        auto* second = launch();
        QVERIFY2(second->waitForStarted(), qPrintable(second->errorString()));
        verifySuccessfulExit(second);
        QVERIFY(!QTest::currentTestFailed());
        QCOMPARE(owner(), originalOwner);
        const QDBusReply<uint> currentPid = bus_->interface()->servicePid(service);
        QVERIFY(currentPid.isValid());
        QCOMPARE(currentPid.value(), originalPid.value());
        QCOMPARE(first->state(), QProcess::Running);
        QVERIFY2(controller.unexpected.isEmpty(), qPrintable(controller.unexpected.join(',')));
        bus_->unregisterObject(QStringLiteral("/controller"));
        QVERIFY(bus_->unregisterService(QStringLiteral("org.fcitx.Fcitx5")));
    }

    void coldDbusActivationAndRepeatedShowReuseOwner() {
        QVERIFY(owner().isEmpty());
        // Unlike claim() forwarding, this is the tray's intentional cold
        // activation call to the well-known name, with autostart enabled.
        const auto message = QDBusMessage::createMethodCall(service, path, interface,
                                                           QStringLiteral("Show"));
        QVERIFY(message.autoStartService());
        QDBusPendingCallWatcher first(bus_->asyncCall(message, 10000));
        QTRY_VERIFY_WITH_TIMEOUT(!owner().isEmpty(), 10000);
        activatedOwner_ = owner();
        const QDBusReply<uint> pid = bus_->interface()->servicePid(activatedOwner_);
        QVERIFY(pid.isValid());
        activatedPid_ = pid.value();
        QVERIFY(activatedPid_ > 0);
        QVERIFY(qint64(activatedPid_) != QCoreApplication::applicationPid());
        QTRY_VERIFY_WITH_TIMEOUT(first.isFinished(), 10000);
        QVERIFY2(!first.isError(), qPrintable(first.error().message()));

        QDBusServiceWatcher watcher(service, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
        QSignalSpy changes(&watcher, &QDBusServiceWatcher::serviceOwnerChanged);
        QDBusPendingCallWatcher second(bus_->asyncCall(message));
        QTRY_VERIFY(second.isFinished());
        QVERIFY2(!second.isError(), qPrintable(second.error().message()));
        QCOMPARE(owner(), activatedOwner_);
        const QDBusReply<uint> repeatedPid = bus_->interface()->servicePid(service);
        QVERIFY(repeatedPid.isValid());
        QCOMPARE(repeatedPid.value(), activatedPid_);
        QCOMPARE(changes.count(), 0);
    }

    void offlineBinaryLeavesServiceAvailable() {
        QDBusServiceWatcher watcher(service, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
        QSignalSpy changes(&watcher, &QDBusServiceWatcher::serviceOwnerChanged);
        auto* offline = launch({QStringLiteral("--offline")});
        QVERIFY2(offline->waitForStarted(), qPrintable(offline->errorString()));
        QTest::qWait(1000); // Offline mode stays open; allow the QML startup to finish.
        QCOMPARE(offline->state(), QProcess::Running);
        QVERIFY(owner().isEmpty());
        QCOMPARE(changes.count(), 0);
        SettingsActivation primary(*bus_);
        QCOMPARE(primary.claim(), SettingsActivation::Claim::Primary);
        QCOMPARE(offline->state(), QProcess::Running);
    }

    void previewBinaryDoesNotClaimOrForward_data() {
        QTest::addColumn<bool>("withPrimary");
        QTest::addColumn<QString>("mode");
        for (const char* mode : {"smoke", "offline-smoke", "screenshot"}) {
            QTest::newRow(qPrintable(QString::fromLatin1(mode) + "-alone"))
                << false << QString::fromLatin1(mode);
            QTest::newRow(qPrintable(QString::fromLatin1(mode) + "-with-primary"))
                << true << QString::fromLatin1(mode);
        }
    }

    void previewBinaryDoesNotClaimOrForward() {
        QFETCH(bool, withPrimary);
        QFETCH(QString, mode);
        SettingsActivation primary(*bus_);
        QSignalSpy requested(&primary, &SettingsActivation::showRequested);
        if (withPrimary)
            QCOMPARE(primary.claim(), SettingsActivation::Claim::Primary);
        const QString originalOwner = owner();
        QDBusServiceWatcher watcher(service, *bus_, QDBusServiceWatcher::WatchForOwnerChange);
        QSignalSpy changes(&watcher, &QDBusServiceWatcher::serviceOwnerChanged);
        const QString screenshot = directory_->filePath(QStringLiteral("preview.png"));
        QStringList arguments;
        if (mode == QStringLiteral("screenshot"))
            arguments = {QStringLiteral("--screenshot"), screenshot};
        else {
            arguments = {QStringLiteral("--smoke-test")};
            if (mode == QStringLiteral("offline-smoke"))
                arguments.append(QStringLiteral("--offline"));
        }
        auto* preview = launch(arguments);
        QVERIFY2(preview->waitForStarted(), qPrintable(preview->errorString()));
        verifySuccessfulExit(preview);
        QVERIFY(!QTest::currentTestFailed());
        QCoreApplication::processEvents();
        QCOMPARE(owner(), originalOwner);
        QCOMPARE(changes.count(), 0); // Includes any temporary claim during startup.
        QCOMPARE(requested.count(), 0);
        if (mode == QStringLiteral("screenshot"))
            QVERIFY(!QImage(screenshot).isNull());
    }
};

int main(int argc, char** argv) {
    if (!qEnvironmentVariableIsSet(privateBusMarker)) {
        QCoreApplication app(argc, argv);
        // dbus-daemon reads activation directories and inherits the GUI
        // environment at startup, before the re-executed test child exists.
        QTemporaryDir fixture;
        if (!fixture.isValid()) return 1;
        const QString runtime = fixture.filePath(QStringLiteral("runtime"));
        const QString config = fixture.filePath(QStringLiteral("config"));
        const QString data = fixture.filePath(QStringLiteral("data"));
        const QString services = QDir(data).filePath(QStringLiteral("dbus-1/services"));
        if (!makePrivateDirectory(runtime) || !makePrivateDirectory(config) ||
            !makePrivateDirectory(services)) return 1;
        QString executable = QString::fromUtf8(OPENKEY_SETTINGS_BINARY);
        executable.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
        executable.replace(QLatin1Char('"'), QStringLiteral("\\\""));
        const QByteArray activation = QStringLiteral(
            "[D-BUS Service]\nName=org.openkey.Settings\nExec=\"%1\"\n")
                .arg(executable).toUtf8();
        QFile serviceFile(QDir(services).filePath(QStringLiteral("org.openkey.Settings.service")));
        if (!serviceFile.open(QIODevice::WriteOnly) ||
            serviceFile.write(activation) != activation.size()) return 1;
        serviceFile.close();
        QProcess runner;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QString::fromLatin1(privateBusMarker), QStringLiteral("1"));
        environment.remove(QStringLiteral("DBUS_SESSION_BUS_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_ADDRESS"));
        environment.remove(QStringLiteral("DBUS_STARTER_BUS_TYPE"));
        environment.insert(QStringLiteral("XDG_RUNTIME_DIR"), runtime);
        environment.insert(QStringLiteral("XDG_CONFIG_HOME"), config);
        environment.insert(QStringLiteral("XDG_DATA_HOME"), data);
        environment.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));
        environment.insert(QStringLiteral("QT_QUICK_BACKEND"), QStringLiteral("software"));
        environment.insert(QStringLiteral("QT_IM_MODULE"), QStringLiteral("compose"));
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

    // Set these before QGuiApplication too, so manual runs are equally isolated.
    QTemporaryDir directory;
    if (!directory.isValid() ||
        !makePrivateDirectory(directory.filePath(QStringLiteral("runtime"))) ||
        !makePrivateDirectory(directory.filePath(QStringLiteral("config"))))
        return 1;
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(directory.filePath(QStringLiteral("runtime"))));
    qputenv("XDG_CONFIG_HOME", QFile::encodeName(directory.filePath(QStringLiteral("config"))));
    qputenv("QT_QPA_PLATFORM", "offscreen");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QT_IM_MODULE", "compose");
    QGuiApplication app(argc, argv);
    app.setQuitOnLastWindowClosed(false);
    LinuxSettingsActivationTests tests;
    return QTest::qExec(&tests, argc, argv);
}

#include "LinuxSettingsActivationTests.moc"
