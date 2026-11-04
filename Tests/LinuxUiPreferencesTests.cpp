#include "UiPreferences.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>

class LinuxUiPreferencesTests : public QObject {
    Q_OBJECT
private slots:
    void backupAndInspectionArePure() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("absent/ui.ini");
        UiPreferences preferences(path);
        QSignalSpy changes(&preferences, &UiPreferences::changed);
        for (const auto& theme : {QString("system"), QString("light"), QString("dark")}) {
            for (bool expanded : {false, true}) {
                const auto json = preferences.backupJson(theme, expanded);
                const auto object = QJsonDocument::fromJson(json.toUtf8()).object();
                QCOMPARE(object.size(), 4);
                QCOMPARE(object.value("format").toString(), "openkey-ui");
                QCOMPARE(object.value("version").toInt(), 1);
                const auto result = preferences.inspectBackup(json);
                QCOMPARE(result.size(), 4);
                QCOMPARE(result.value("ok").toBool(), true);
                QCOMPARE(result.value("status").toString(), "ready");
                QCOMPARE(result.value("theme").toString(), theme);
                QCOMPARE(result.value("sidebarExpanded").toBool(), expanded);
            }
        }
        QVERIFY(preferences.backupJson("unknown", false).isEmpty());
        QCOMPARE(preferences.status(), "not-read");
        QCOMPARE(preferences.themeMode(), "system");
        QVERIFY(preferences.sidebarExpanded());
        QCOMPARE(changes.count(), 0);
        QVERIFY(!QFile::exists(directory.filePath("absent")));

        // A FIFO would block if either pure operation tried to read preferences.
        const auto fifo = directory.filePath("fifo");
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
        UiPreferences special(fifo);
        QSignalSpy specialChanges(&special, &UiPreferences::changed);
        QVERIFY(special.inspectBackup(special.backupJson("dark", false)).value("ok").toBool());
        QCOMPARE(special.status(), "not-read");
        QCOMPARE(specialChanges.count(), 0);
    }
    void invalidBackup_data() {
        QTest::addColumn<QString>("json");
        QTest::addColumn<QString>("status");
        const QString valid = R"({"format":"openkey-ui","version":1,"theme":"dark","sidebarExpanded":false})";
        const auto invalid = [](const char* name, const QString& json) {
            QTest::newRow(name) << json << QString("invalid");
        };
        invalid("empty", "");
        invalid("malformed", "{");
        invalid("array", "[]");
        invalid("null", "null");
        invalid("missing", R"({"format":"openkey-ui","version":1,"theme":"dark"})");
        invalid("unknown-field", valid.left(valid.size() - 1) + R"(,"path":"/tmp/elsewhere"})");
        for (const auto& field : {QString("format"), QString("version"), QString("theme"), QString("sidebarExpanded")}) {
            const auto base = QJsonDocument::fromJson(valid.toUtf8()).object();
            auto missing = base;
            missing.remove(field);
            const auto missingName = (field + "-missing").toUtf8();
            invalid(missingName.constData(), QString::fromUtf8(QJsonDocument(missing).toJson()));
            for (const auto& value : {QJsonValue(), QJsonValue(QJsonArray()), QJsonValue(QJsonObject()),
                                      QJsonValue("wrong"), QJsonValue(2), QJsonValue(true)}) {
                if (field == "sidebarExpanded" && value.isBool()) continue;
                auto object = base;
                object[field] = value;
                const auto name = (field + QString::number(value.type())).toUtf8();
                invalid(name.constData(), QString::fromUtf8(QJsonDocument(object).toJson()));
            }
        }
        invalid("format", QString(valid).replace("openkey-ui", "openkey"));
        invalid("theme", QString(valid).replace("dark", "Dark"));
        invalid("version-string", QString(valid).replace(":1,", ":\"1\","));
        invalid("version-fraction", QString(valid).replace(":1,", ":1.5,"));
        invalid("version-future", QString(valid).replace(":1,", ":2,"));
        invalid("sidebar-string", QString(valid).replace("false", "\"false\""));
        invalid("sidebar-number", QString(valid).replace("false", "0"));
        invalid("theme-number", QString(valid).replace("\"dark\"", "1"));
        invalid("nul", valid + QChar(0));
        invalid("high-surrogate", valid + QChar(0xd800));
        invalid("low-surrogate", valid + QChar(0xdc00));
        invalid("broken-pair", valid + QChar(0xd800) + " ");
        invalid("embedded-nul", QString(valid).insert(2, QChar(0)));
        QTest::newRow("too-large") << valid + QString(4097 - valid.size(), ' ') << QString("too-large");
        QTest::newRow("utf16-limit") << valid + QString(4096 - valid.size(), QChar(0xd800)) + QChar(0xdc00)
                                    << QString("too-large");
    }
    void invalidBackup() {
        QFETCH(QString, json);
        QFETCH(QString, status);
        QTemporaryDir directory;
        UiPreferences preferences(directory.filePath("absent/ui.ini"));
        QSignalSpy changes(&preferences, &UiPreferences::changed);
        const auto result = preferences.inspectBackup(json);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result.value("ok").toBool(), false);
        QCOMPARE(result.value("status").toString(), status);
        QVERIFY(!preferences.restoreBackup(json));
        QCOMPARE(preferences.status(), "not-read");
        QCOMPARE(preferences.themeMode(), "system");
        QVERIFY(preferences.sidebarExpanded());
        QCOMPARE(changes.count(), 0);
        QVERIFY(!QFile::exists(directory.filePath("absent")));
    }
    void backupRestoreRoundTrip() {
        QTemporaryDir sourceDirectory, destinationDirectory;
        UiPreferences source(sourceDirectory.filePath("absent/ui.ini"));
        const auto json = source.backupJson("light", false);
        UiPreferences destination(destinationDirectory.filePath("ui.ini"));
        const auto padded = json + QString(4096 - json.size(), ' ');
        QVERIFY(destination.inspectBackup(padded).value("ok").toBool());
        QVERIFY(!QFile::exists(destinationDirectory.filePath("ui.ini")));
        QSignalSpy changes(&destination, &UiPreferences::changed);
        QList<QString> observed;
        connect(&destination, &UiPreferences::changed, &destination, [&] {
            observed.append(destination.themeMode() + (destination.sidebarExpanded() ? ":true" : ":false"));
        });
        QVERIFY(destination.restoreBackup(padded));
        QCOMPARE(changes.count(), 1);
        QCOMPARE(observed, QList<QString>{"light:false"});
        QCOMPARE(destination.status(), "saved");
        UiPreferences reopened(destinationDirectory.filePath("ui.ini"));
        reopened.load();
        QCOMPARE(reopened.themeMode(), "light");
        QVERIFY(!reopened.sidebarExpanded());
        QSettings settings(destinationDirectory.filePath("ui.ini"), QSettings::IniFormat);
        QCOMPARE(settings.value("Interface/Theme").toString(), "light");
        QCOMPARE(settings.value("Interface/SidebarExpanded").toBool(), false);
        QVERIFY(!QFile::exists(sourceDirectory.filePath("absent")));

        // Inspection is advisory: edited text must be parsed again on restore.
        QFile disk(destinationDirectory.filePath("ui.ini"));
        QVERIFY(disk.open(QIODevice::ReadOnly));
        const auto bytes = disk.readAll();
        disk.close();
        changes.clear();
        QVERIFY(!destination.restoreBackup(QString(json).replace("light", "unknown")));
        QCOMPARE(changes.count(), 0);
        QCOMPARE(destination.status(), "saved");
        QVERIFY(disk.open(QIODevice::ReadOnly));
        QCOMPARE(disk.readAll(), bytes);
    }
    void restoreExistingFile() {
        QTemporaryDir directory;
        const auto path = directory.filePath("ui.ini");
        {
            QSettings settings(path, QSettings::IniFormat);
            settings.setValue("FormatVersion", 1);
            settings.setValue("Interface/Theme", "dark");
            settings.setValue("Interface/SidebarExpanded", false);
            settings.setValue("Other/Untouched", "keep");
            settings.sync();
            QCOMPARE(settings.status(), QSettings::NoError);
        }
        UiPreferences preferences(path);
        QSignalSpy changes(&preferences, &UiPreferences::changed);
        connect(&preferences, &UiPreferences::changed, &preferences, [&] {
            QCOMPARE(preferences.themeMode(), "light");
            QVERIFY(preferences.sidebarExpanded());
            QCOMPARE(preferences.status(), "saved");
        });
        QVERIFY(preferences.restoreBackup(preferences.backupJson("light", true)));
        QCOMPARE(changes.count(), 1);
        UiPreferences reopened(path);
        reopened.load();
        QCOMPARE(reopened.themeMode(), "light");
        QVERIFY(reopened.sidebarExpanded());
        QSettings settings(path, QSettings::IniFormat);
        QCOMPARE(settings.value("Other/Untouched").toString(), "keep");
    }
    void restoreFailures_data() {
        QTest::addColumn<QString>("kind");
        for (const auto* kind : {"invalid", "symlink", "fifo", "parent-file", "blocked"})
            QTest::newRow(kind) << QString(kind);
    }
    void restoreFailures() {
        QFETCH(QString, kind);
        QTemporaryDir directory;
        const auto path = directory.filePath("ui.ini");
        UiPreferences preferences(path);
        QVERIFY(preferences.setThemeMode("dark"));
        QVERIFY(preferences.setSidebarExpanded(false));
        QVERIFY(QFile::remove(path));
        const QByteArray oldBytes("FormatVersion=2\n[Interface]\nTheme=dark\n");
        auto preservedPath = path;
        if (kind == "symlink") {
            preservedPath = directory.filePath("target.ini");
        } else if (kind == "parent-file") {
            // Replace the whole parent directory after loading non-default values.
            QVERIFY(QDir(directory.path()).removeRecursively());
            preservedPath = directory.path();
        }
        if (kind == "fifo") {
            QCOMPARE(::mkfifo(QFile::encodeName(path).constData(), 0600), 0);
        } else {
            QFile file(preservedPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(oldBytes), oldBytes.size());
            file.close();
            if (kind == "symlink") QVERIFY(QFile::link(preservedPath, path));
        }
        if (kind == "blocked") {
            preferences.load();
            QCOMPARE(preferences.status(), "invalid");
            // Even after external repair, a blocked file cannot be overwritten.
            QFile repaired(path);
            QVERIFY(repaired.open(QIODevice::WriteOnly | QIODevice::Truncate));
            const QByteArray repairedBytes("[Interface]\nTheme=dark\n");
            QCOMPARE(repaired.write(repairedBytes), repairedBytes.size());
        }
        // An unread invalid file must also be loaded and refused.
        if (kind == "invalid") {
            UiPreferences unread(path);
            QSignalSpy unreadChanges(&unread, &UiPreferences::changed);
            QVERIFY(!unread.restoreBackup(unread.backupJson("light", true)));
            QCOMPARE(unread.status(), "invalid");
            QCOMPARE(unread.themeMode(), "system");
            QVERIFY(unread.sidebarExpanded());
            QCOMPARE(unreadChanges.count(), 1);
            // Mark the existing instance blocked through its normal load path.
            preferences.load();
        }
        const auto oldTheme = preferences.themeMode();
        const auto oldExpanded = preferences.sidebarExpanded();
        QByteArray bytes;
        if (kind != "fifo") {
            QFile file(preservedPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            bytes = file.readAll();
        }
        QSignalSpy changes(&preferences, &UiPreferences::changed);
        connect(&preferences, &UiPreferences::changed, &preferences, [&] {
            QCOMPARE(preferences.themeMode(), oldTheme);
            QCOMPARE(preferences.sidebarExpanded(), oldExpanded);
        });
        QVERIFY(!preferences.restoreBackup(preferences.backupJson("light", true)));
        QCOMPARE(preferences.themeMode(), oldTheme);
        QCOMPARE(preferences.sidebarExpanded(), oldExpanded);
        QCOMPARE(preferences.status(), kind == "parent-file" ? "error" : "invalid");
        QCOMPARE(changes.count(), 1);
        if (kind != "fifo") {
            QFile file(preservedPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), bytes);
        } else {
            struct stat info;
            QCOMPARE(::lstat(QFile::encodeName(path).constData(), &info), 0);
            QVERIFY(S_ISFIFO(info.st_mode));
        }
        if (kind == "parent-file") QVERIFY(QFile::remove(preservedPath));
    }
    void missingDoesNotCreate() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const auto path = directory.filePath("openkey/ui.ini");
        UiPreferences preferences(path);
        QCOMPARE(preferences.status(), "not-read");
        preferences.load();
        QCOMPARE(preferences.status(), "missing");
        QCOMPARE(preferences.themeMode(), "system");
        QVERIFY(preferences.sidebarExpanded());
        QVERIFY(!QFile::exists(directory.filePath("openkey")));
        QVERIFY(!preferences.setThemeMode("arbitrary"));
        QVERIFY(!QFile::exists(path));
    }
    void roundTrip() {
        QTemporaryDir directory;
        const auto path = directory.filePath("openkey/ui.ini");
        UiPreferences preferences(path);
        preferences.load();
        QVERIFY(preferences.setThemeMode("dark"));
        QVERIFY(preferences.setSidebarExpanded(false));
        QCOMPARE(preferences.status(), "saved");
        UiPreferences reopened(path);
        reopened.load();
        QCOMPARE(reopened.status(), "loaded");
        QCOMPARE(reopened.themeMode(), "dark");
        QVERIFY(!reopened.sidebarExpanded());
        QVERIFY(reopened.setThemeMode("light"));
        QVERIFY(!reopened.sidebarExpanded());
        QVERIFY(reopened.setThemeMode("system"));
        QVERIFY(reopened.setSidebarExpanded(true));
        UiPreferences again(path);
        again.load();
        QCOMPARE(again.themeMode(), "system");
        QVERIFY(again.sidebarExpanded());
    }
    void invalidFile_data() {
        QTest::addColumn<QByteArray>("data");
        QTest::newRow("unsupported-version") << QByteArray("FormatVersion=2\n[Interface]\nTheme=dark\n");
        QTest::newRow("invalid-theme") << QByteArray("[Interface]\nTheme=unknown\n");
        QTest::newRow("invalid-sidebar") << QByteArray("[Interface]\nSidebarExpanded=perhaps\n");
        QTest::newRow("malformed") << QByteArray("[Broken\nTheme=dark\n");
        QTest::newRow("oversized") << QByteArray(16385, 'x');
    }
    void invalidFile() {
        QFETCH(QByteArray, data);
        QTemporaryDir directory;
        const auto path = directory.filePath("ui.ini");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(data), data.size());
        file.close();
        UiPreferences preferences(path);
        preferences.load();
        QCOMPARE(preferences.status(), "invalid");
        QCOMPARE(preferences.themeMode(), "system");
        QVERIFY(preferences.sidebarExpanded());
        QVERIFY(!preferences.setThemeMode("light"));
        QVERIFY(!preferences.setSidebarExpanded(false));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
    }
    void specialFiles() {
        QTemporaryDir directory;
        const auto target = directory.filePath("target.ini");
        const auto link = directory.filePath("ui.ini");
        QFile file(target);
        QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray data("[Interface]\nTheme=dark\n");
        QCOMPARE(file.write(data), data.size());
        file.close();
        QVERIFY(QFile::link(target, link));
        UiPreferences preferences(link);
        preferences.load();
        QCOMPARE(preferences.status(), "invalid");
        QVERIFY(!preferences.setThemeMode("light"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), data);
        const auto fifo = directory.filePath("fifo");
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
        UiPreferences special(fifo);
        special.load();
        QCOMPARE(special.status(), "invalid");
        QVERIFY(!special.setThemeMode("light"));
    }
    void cannotCreateParent() {
        QTemporaryDir directory;
        const auto parent = directory.filePath("not-a-directory");
        QFile file(parent);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        UiPreferences preferences(parent + "/ui.ini");
        preferences.load();
        QVERIFY(!preferences.setThemeMode("dark"));
        QCOMPARE(preferences.status(), "error");
        QVERIFY(!preferences.setSidebarExpanded(false));
        QCOMPARE(preferences.themeMode(), "dark");
        QVERIFY(!preferences.sidebarExpanded());
    }
};
QTEST_GUILESS_MAIN(LinuxUiPreferencesTests)
#include "LinuxUiPreferencesTests.moc"
