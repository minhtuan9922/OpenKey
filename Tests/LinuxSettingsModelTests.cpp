#include "SettingsOverview.h"
#include <QDir>
#include <QFile>
#include <QLocalServer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

class LinuxSettingsModelTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString runtime, config, socketPath;
    void writeConfig(const QByteArray& data) {
        QFile file(config);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(data), data.size());
    }
    void listen(QLocalServer& server) {
        QVERIFY(server.listen(socketPath));
        QCOMPARE(::chmod(QFile::encodeName(socketPath).constData(), 0600), 0);
    }
private slots:
    void init() {
        QVERIFY(directory.isValid());
        runtime = directory.path();
        config = runtime + "/config";
        socketPath = runtime + "/openkey/session.sock";
        QVERIFY(QDir().mkpath(runtime + "/openkey"));
        QCOMPARE(::chmod(QFile::encodeName(runtime + "/openkey").constData(), 0700), 0);
    }
    void cleanup() {
        QFile::remove(config);
        QFile::remove(socketPath);
    }
    void offlineConstructor() {
        SettingsOverview model(runtime, config);
        QCOMPARE(model.serviceState(), "disabled");
        QCOMPARE(model.configState(), "not-read");
        QVERIFY(!QFile::exists(config));
    }
    void configReadOnly() {
        const QByteArray data = "# comment\nInputStyle=Direct\nDirectDiagnostics=True\nFutureOption=keep\n";
        writeConfig(data);
        SettingsOverview model(runtime, config);
        model.refresh();
        QCOMPARE(model.configState(), "read");
        QCOMPARE(model.inputStyle(), "direct");
        QCOMPARE(model.diagnostics(), "enabled");
        QCOMPARE(model.serviceState(), "unavailable");
        QFile file(config);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
        file.close();
        writeConfig("InputStyle=Preedit\nDirectDiagnostics=False\n");
        model.refresh();
        QCOMPARE(model.inputStyle(), "preedit");
        QCOMPARE(model.diagnostics(), "disabled");
        QVERIFY(QFile::remove(config));
        model.refresh();
        QCOMPARE(model.configState(), "missing");
        QCOMPARE(model.inputStyle(), "unknown");
        QCOMPARE(model.diagnostics(), "unknown");
        QVERIFY(!QFile::exists(config));
    }
    void configValidation_data() {
        QTest::addColumn<QByteArray>("data");
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("style");
        QTest::newRow("missing-key") << QByteArray("# empty\n") << QString("read") << QString("unknown");
        QTest::newRow("commit-only") << QByteArray("InputStyle=CommitOnly\n") << QString("read") << QString("commit-only");
        QTest::newRow("native-direct") << QByteArray("InputStyle=NativeDirect\n") << QString("read") << QString("native-direct");
        QTest::newRow("invalid-enum") << QByteArray("InputStyle=Fake\n") << QString("invalid") << QString("unknown");
        QTest::newRow("duplicate") << QByteArray("InputStyle=Direct\nInputStyle=Preedit\n") << QString("invalid") << QString("unknown");
        QTest::newRow("bad-boolean") << QByteArray("InputStyle=Direct\nDirectDiagnostics=yes\n") << QString("invalid") << QString("unknown");
        QTest::newRow("missing-equals") << QByteArray("InputStyle\n") << QString("invalid") << QString("unknown");
        QTest::newRow("oversized") << QByteArray(65537, 'x') << QString("unreadable") << QString("unknown");
        QTest::newRow("null-byte") << QByteArray("InputStyle=Direct\0", 18) << QString("invalid") << QString("unknown");
        QTest::newRow("section-not-root") << QByteArray("[Other]\nInputStyle=Direct\n") << QString("read") << QString("unknown");
    }
    void configValidation() {
        QFETCH(QByteArray, data);
        QFETCH(QString, state);
        QFETCH(QString, style);
        writeConfig(data);
        SettingsOverview model(runtime, config);
        model.refresh();
        QCOMPARE(model.configState(), state);
        QCOMPARE(model.inputStyle(), style);
    }
    void unsafeConfig() {
        QCOMPARE(::mkfifo(QFile::encodeName(config).constData(), 0600), 0);
        SettingsOverview model(runtime, config);
        model.refresh();
        QCOMPARE(model.configState(), "unreadable");
        QVERIFY(QFile::remove(config));
        QVERIFY(QFile::link(runtime + "/nonexistent", config));
        model.refresh();
        QCOMPARE(model.configState(), "unreadable");
    }
    void ipc_data() {
        QTest::addColumn<QByteArray>("reply");
        QTest::addColumn<QString>("state");
        QTest::addColumn<QString>("input");
        for (const auto& value : {"telex", "vni", "english", "inactive", "other", "unknown", "unavailable"})
            QTest::newRow(value) << QByteArray(value) + '\n' << QString("connected") << QString(value);
        QTest::newRow("unknown-wire-value") << QByteArray("sensitive text\n") << QString("protocol-error") << QString("unknown");
        QTest::newRow("two-lines") << QByteArray("telex\nvni\n") << QString("protocol-error") << QString("unknown");
        QTest::newRow("oversized") << QByteArray(40, 'x') << QString("protocol-error") << QString("unknown");
        QTest::newRow("truncated") << QByteArray("telex") << QString("unavailable") << QString("unknown");
    }
    void ipc() {
        QFETCH(QByteArray, reply);
        QFETCH(QString, state);
        QFETCH(QString, input);
        QLocalServer server;
        listen(server);
        QByteArray request;
        connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto* client = server.nextPendingConnection();
            connect(client, &QLocalSocket::readyRead, &server, [&, client] {
                request += client->readAll();
                if (!request.contains('\n')) return;
                client->write(reply);
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        });
        SettingsOverview model(runtime, config);
        model.refresh();
        QTRY_COMPARE(model.serviceState(), state);
        QCOMPARE(model.inputStatus(), input);
        QCOMPARE(request, "input-status\n");
        // A lost service must invalidate the previous sample on the next poll.
        server.close();
        model.refresh();
        QTRY_COMPARE(model.serviceState(), QString("unavailable"));
        QCOMPARE(model.inputStatus(), "unknown");
    }
    void timeoutAndRecovery() {
        QLocalServer server;
        listen(server);
        SettingsOverview model(runtime, config);
        model.refresh();
        QTRY_VERIFY(server.hasPendingConnections());
        std::unique_ptr<QLocalSocket> first(server.nextPendingConnection());
        model.refresh(); // Do not queue concurrent requests while one is pending.
        QTest::qWait(20);
        QVERIFY(!server.hasPendingConnections());
        QTRY_COMPARE(model.serviceState(), QString("timeout"));
        QCOMPARE(model.inputStatus(), "unknown");
        model.refresh();
        QTRY_VERIFY(server.hasPendingConnections());
        std::unique_ptr<QLocalSocket> second(server.nextPendingConnection());
        QTRY_VERIFY(second->bytesAvailable() > 0);
        QCOMPARE(second->readAll(), "input-status\n");
        second->write("vn");
        second->flush();
        QTest::qWait(20);
        QVERIFY(model.serviceState() != "connected");
        second->write("i\n");
        second->flush();
        QTRY_COMPARE(model.inputStatus(), QString("vni"));
        QCOMPARE(model.serviceState(), "connected");
    }
    void insecureRuntime() {
        QLocalServer server;
        listen(server);
        SettingsOverview model(runtime, config);
        QCOMPARE(::chmod(QFile::encodeName(runtime + "/openkey").constData(), 0755), 0);
        model.refresh();
        QCOMPARE(model.serviceState(), "unavailable");
        QTest::qWait(30);
        QVERIFY(!server.hasPendingConnections());
        QCOMPARE(::chmod(QFile::encodeName(runtime + "/openkey").constData(), 0700), 0);
    }
    void automaticPolling() {
        QLocalServer server;
        listen(server);
        int requests = 0;
        connect(&server, &QLocalServer::newConnection, &server, [&] {
            auto* client = server.nextPendingConnection();
            connect(client, &QLocalSocket::readyRead, &server, [&, client] {
                const auto request = client->readAll();
                if (request != "input-status\n") return;
                ++requests;
                client->write(requests == 1 ? "telex\n" : "english\n");
                client->disconnectFromServer();
            });
            connect(client, &QLocalSocket::disconnected, client, &QObject::deleteLater);
        });
        SettingsOverview model(runtime, config);
        model.start();
        QTRY_COMPARE(model.inputStatus(), QString("telex"));
        QTRY_COMPARE(model.inputStatus(), QString("english"));
        QVERIFY(requests >= 2);
        server.close();
        QTRY_COMPARE(model.inputStatus(), QString("unknown"));
        QCOMPARE(model.serviceState(), "unavailable");
    }
};
QTEST_GUILESS_MAIN(LinuxSettingsModelTests)
#include "LinuxSettingsModelTests.moc"
