#include "BackupFiles.h"
#include "ApplicationMemoryBackup.h"
#include "../Sources/OpenKey/linux/ApplicationMemory.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "InputConfigBackup.h"
#include "UiPreferences.h"
#include "MacroModel.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
#include <sys/stat.h>

class LinuxBackupFilesTests : public QObject {
    Q_OBJECT
    BackupFiles files;
    QString json(const QString& style = "Direct") const {
        return InputConfigBackup::encode({{"InputStyle", style}, {"DirectDiagnostics", "False"}});
    }
    QByteArray read(const QString& path) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {};
        return file.readAll();
    }
private slots:
    void applicationMemoryPlans() {
        openkey::ApplicationMemory current, incoming;
        current.remember("code", "keyboard-us"); current.remember("keep", "openkey-vni");
        incoming.remember("code", "openkey-telex"); incoming.remember("gnome:google-chrome.desktop", "openkey-simple-telex1");
        const auto text = ApplicationMemoryBackup::encode(QString::fromStdString(incoming.encode()));
        openkey::MacroTable::Entries decoded;
        QVERIFY(ApplicationMemoryBackup::decode(text, decoded)); QCOMPARE(decoded.size(), std::size_t(2));
        const auto merge = ApplicationMemoryBackup::plan(text, "merge", QString::fromStdString(current.encode()));
        QVERIFY(merge.value("ok").toBool()); QCOMPARE(merge.value("added").toInt(), 1);
        QCOMPARE(merge.value("updated").toInt(), 1); QCOMPARE(merge.value("removed").toInt(), 0); QCOMPARE(merge.value("total").toInt(), 3);
        const auto replace = ApplicationMemoryBackup::plan(text, "replace", QString::fromStdString(current.encode()));
        QCOMPARE(replace.value("removed").toInt(), 1); QCOMPARE(replace.value("total").toInt(), 2);
        const auto empty = ApplicationMemoryBackup::encode("");
        const auto clear = ApplicationMemoryBackup::plan(empty, "replace", QString::fromStdString(current.encode()));
        QVERIFY(clear.value("ok").toBool()); QCOMPARE(clear.value("removed").toInt(), 2); QCOMPARE(clear.value("target").toString(), QString(""));
        QCOMPARE(ApplicationMemoryBackup::plan(empty, "merge", QString::fromStdString(current.encode())).value("changes").toList().size(), 0);
        QVERIFY(!ApplicationMemoryBackup::plan(text, "unknown", "").value("ok").toBool());
        for (int i = 0; i < 128; ++i) current.remember("app-" + std::to_string(i), "keyboard-us");
        QCOMPARE(ApplicationMemoryBackup::plan(text, "merge", QString::fromStdString(current.encode())).value("status").toString(), "capacity");
        QVERIFY(ApplicationMemoryBackup::plan(text, "replace", QString::fromStdString(current.encode())).value("ok").toBool());
        auto root = QJsonDocument::fromJson(text.toUtf8()).object();
        auto rows = root.value("entries").toArray(); rows.append(rows.first()); root["entries"] = rows;
        QVERIFY(!ApplicationMemoryBackup::decode(QString::fromUtf8(QJsonDocument(root).toJson()), decoded));
        QVERIFY(!ApplicationMemoryBackup::decode(json(), decoded));
        QVERIFY(!ApplicationMemoryBackup::decode(QString(65537, ' '), decoded));
        for (const auto& key : QStringList{"format", "version", "entries"}) {
            auto broken = QJsonDocument::fromJson(text.toUtf8()).object(); broken[key] = false;
            QVERIFY(!ApplicationMemoryBackup::decode(QString::fromUtf8(QJsonDocument(broken).toJson()), decoded));
        }
        for (const auto& app : QStringList{"gnome-shell", "unknown", "bad app"}) {
            QJsonObject bad{{"format", "openkey-application-memory"}, {"version", 1},
                {"entries", QJsonArray{QJsonObject{{"application", app}, {"method", "keyboard-us"}}}}};
            QVERIFY(!ApplicationMemoryBackup::decode(QString::fromUtf8(QJsonDocument(bad).toJson()), decoded));
        }
    }
    void applicationMemoryFiles() {
        QTemporaryDir dir;
        const auto text = ApplicationMemoryBackup::encode("");
        const auto url = QUrl::fromLocalFile(dir.filePath("ứng dụng.json"));
        QVERIFY(files.validApplicationMemoryBackup(text));
        QCOMPARE(files.saveApplicationMemory(url, text), "saved");
        QCOMPARE(files.loadApplicationMemory(url).value("text").toString(), text);
        QVERIFY(!files.load(url).value("ok").toBool()); QVERIFY(!files.loadMacros(url).value("ok").toBool());
        QCOMPARE(files.saveApplicationMemory(url, json()), "invalid-json"); QCOMPARE(read(url.toLocalFile()), text.toUtf8());
        QVERIFY(!files.loadApplicationMemory(QUrl("https://example.com/test.json")).value("ok").toBool());
        QVERIFY(QFile::link(url.toLocalFile(), dir.filePath("link.json")));
        QVERIFY(!files.loadApplicationMemory(QUrl::fromLocalFile(dir.filePath("link.json"))).value("ok").toBool());
        const auto tooLarge = dir.filePath("large.json"); QFile file(tooLarge); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(131073, ' ')); file.close();
        QVERIFY(!files.loadApplicationMemory(QUrl::fromLocalFile(tooLarge)).value("ok").toBool());
    }
    void methodAndHotkeyFiles() {
        QTemporaryDir dir;
        const QString method = "{\"format\":\"openkey-input-method\",\"version\":1,\"method\":\"openkey-simple-telex1\"}";
        QVERIFY(files.validMethodBackup(method));
        QVERIFY(!files.validHotkeyBackup(method));
        QVERIFY(!files.validBackup(method));
        const auto url = QUrl::fromLocalFile(dir.filePath("bộ gõ.json"));
        QCOMPARE(files.saveMethod(url, method), "saved");
        QCOMPARE(files.loadMethod(url).value("text").toString(), method);
        QVERIFY(!files.loadHotkeys(url).value("ok").toBool());
        QCOMPARE(files.saveMethod(url, json()), "invalid-json");
        QCOMPARE(read(url.toLocalFile()), method.toUtf8());
        const QString hotkeys = "{\"format\":\"openkey-hotkeys\",\"version\":1,\"shortcuts\":{\"TriggerKeys\":[\"Control+space\"],\"AltTriggerKeys\":[],\"ActivateKeys\":[],\"DeactivateKeys\":[],\"EnumerateForwardKeys\":[],\"EnumerateBackwardKeys\":[],\"EnumerateGroupForwardKeys\":[],\"EnumerateGroupBackwardKeys\":[]}}";
        QVERIFY(files.validHotkeyBackup(hotkeys));
        QCOMPARE(files.saveHotkeys(url, hotkeys), "saved");
        QCOMPARE(files.loadHotkeys(url).value("text").toString(), hotkeys);
        QVERIFY(!files.loadMethod(url).value("ok").toBool());
        QVERIFY(!files.validHotkeyBackup("{}"));
    }
    void macroRoundtripAndIsolation() {
        QTemporaryDir dir;
        MacroModel macros;
        QVERIFY(macros.put("vn", "Việt Nam\nxin chào"));
        const auto text = macros.exportJson();
        const auto url = QUrl::fromLocalFile(dir.filePath("gõ tắt.json"));
        QCOMPARE(files.saveMacros(url, text), "saved");
        QCOMPARE(files.loadMacros(url).value("text").toString(), text);
        QVERIFY(!files.load(url).value("ok").toBool());
        QVERIFY(!files.loadUi(url).value("ok").toBool());
        QCOMPARE(files.saveMacros(url, json()), "invalid-json");
        QCOMPARE(read(url.toLocalFile()), text.toUtf8());
        QVERIFY(!files.loadMacros(QUrl("https://example.com/table.json")).value("ok").toBool());
        QVERIFY(QFile::link(url.toLocalFile(), dir.filePath("link.json")));
        QVERIFY(!files.loadMacros(QUrl::fromLocalFile(dir.filePath("link.json"))).value("ok").toBool());
    }
    void uiRoundtripAndFormatIsolation() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        UiPreferences preferences(dir.filePath("ui.ini"));
        const auto uiJson = preferences.backupJson("dark", false);
        QVERIFY(files.validUiBackup(uiJson));
        QVERIFY(!files.validUiBackup(json()));
        QVERIFY(!files.validBackup(uiJson));
        const auto url = QUrl::fromLocalFile(dir.filePath("giao diện.json"));
        QCOMPARE(files.saveUi(url, uiJson), "saved");
        QCOMPARE(files.loadUi(url).value("text").toString(), uiJson);
        QVERIFY(!files.load(url).value("ok").toBool());
        QCOMPARE(files.save(url, uiJson), "invalid-json");
        QCOMPARE(files.saveUi(url, json()), "invalid-json");
        QCOMPARE(read(url.toLocalFile()), uiJson.toUtf8());
        QCOMPARE(files.save(url, json()), "saved");
        QVERIFY(!files.loadUi(url).value("ok").toBool());
        QCOMPARE(read(url.toLocalFile()), json().toUtf8());
        QVERIFY(!QFile::exists(dir.filePath("ui.ini"))); // File I/O never restores UI state.
    }
    void uiInvalidFile_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("unknown-theme") << QByteArray("{\"format\":\"openkey-ui\",\"version\":1,\"theme\":\"invalid\",\"sidebarExpanded\":true}");
        QTest::newRow("wrong-type") << QByteArray("{\"format\":\"openkey-ui\",\"version\":1,\"theme\":\"dark\",\"sidebarExpanded\":\"false\"}");
        QTest::newRow("oversized") << QByteArray(16385, ' ');
        QTest::newRow("incomplete-utf8") << QByteArray("{}") + QByteArray::fromHex("c3");
    }
    void uiInvalidFile() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("bad.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        const auto loaded = files.loadUi(QUrl::fromLocalFile(path));
        QVERIFY(!loaded.value("ok").toBool());
        QVERIFY(!loaded.contains("text"));
        QCOMPARE(read(path), bytes);
    }
    void roundtripAndReplace() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath(QString::fromUtf8("bản sao #1%.json"));
        const auto url = QUrl::fromLocalFile(path);
        QCOMPARE(files.save(url, json()), "saved");
        QCOMPARE(read(path), json().toUtf8());
        const auto loaded = files.load(url);
        QVERIFY(loaded.value("ok").toBool());
        QCOMPARE(loaded.value("text").toString(), json());
        QCOMPARE(files.save(url, json("NativeDirect")), "saved");
        QCOMPARE(read(path), json("NativeDirect").toUtf8());
        // Invalid output must not truncate an existing selected file.
        QCOMPARE(files.save(url, "{}"), "invalid-json");
        QCOMPARE(read(path), json("NativeDirect").toUtf8());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot).size(), 1);
        QFile bomFile(path);
        QVERIFY(bomFile.open(QIODevice::WriteOnly));
        const auto bytes = QByteArray::fromHex("efbbbf") + json().toUtf8();
        QCOMPARE(bomFile.write(bytes), bytes.size());
        bomFile.close();
        QCOMPARE(files.load(url).value("text").toString(), json());
    }
    void invalidUrl_data() {
        QTest::addColumn<QUrl>("url");
        QTest::newRow("empty") << QUrl();
        QTest::newRow("relative") << QUrl("backup.json");
        QTest::newRow("network") << QUrl("https://example.invalid/backup.json");
        QTest::newRow("file-host") << QUrl("file://example.invalid/backup.json");
        QTest::newRow("query") << QUrl("file:///tmp/backup.json?x=1");
        QTest::newRow("fragment") << QUrl("file:///tmp/backup.json#fragment");
        QTest::newRow("nul") << QUrl("file:///tmp/backup%00.json");
    }
    void invalidUrl() {
        QFETCH(QUrl, url);
        const auto loaded = files.load(url);
        QVERIFY(!loaded.value("ok").toBool());
        QVERIFY(!loaded.contains("text"));
        QCOMPARE(loaded.value("status").toString(), "invalid-path");
        QCOMPARE(files.save(url, json()), "invalid-path");
    }
    void invalidFile_data() {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("not-json") << QByteArray("not JSON");
        QTest::newRow("wrong-schema") << QByteArray("{\"theme\":\"dark\"}");
        QTest::newRow("large-text") << json().toUtf8() + QByteArray(4096, ' ');
        QTest::newRow("large-file") << QByteArray(16385, ' ');
        QTest::newRow("invalid-utf8") << json().toUtf8() + QByteArray::fromHex("ff");
        QTest::newRow("incomplete-utf8") << json().toUtf8() + QByteArray::fromHex("c3");
    }
    void invalidFile() {
        QFETCH(QByteArray, bytes);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto path = dir.filePath("input.json");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(bytes), bytes.size());
        file.close();
        const auto loaded = files.load(QUrl::fromLocalFile(path));
        QVERIFY(!loaded.value("ok").toBool());
        QVERIFY(!loaded.contains("text"));
        QCOMPARE(read(path), bytes);
    }
    void missingAndSpecialFiles() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!files.load(QUrl::fromLocalFile(dir.filePath("missing.json"))).value("ok").toBool());
        QVERIFY(!files.load(QUrl::fromLocalFile(dir.path())).value("ok").toBool());
        QCOMPARE(files.save(QUrl::fromLocalFile(dir.path()), json()), "invalid-file");
        const auto regular = dir.filePath("backup.json");
        QCOMPARE(files.save(QUrl::fromLocalFile(regular), json()), "saved");
        const auto link = dir.filePath("link.json");
        QVERIFY(QFile::link(regular, link));
        QVERIFY(!files.load(QUrl::fromLocalFile(link)).value("ok").toBool());
        QCOMPARE(files.save(QUrl::fromLocalFile(link), json("Preedit")), "invalid-file");
        QCOMPARE(read(regular), json().toUtf8());
        const auto fifo = dir.filePath("fifo.json");
        QCOMPARE(::mkfifo(QFile::encodeName(fifo).constData(), 0600), 0);
        QVERIFY(!files.load(QUrl::fromLocalFile(fifo)).value("ok").toBool());
        QCOMPARE(files.save(QUrl::fromLocalFile(fifo), json()), "invalid-file");
        QCOMPARE(files.save(QUrl::fromLocalFile(regular + "/child.json"), json()), "unwritable");
        QCOMPARE(read(regular), json().toUtf8());
    }
};
QTEST_GUILESS_MAIN(LinuxBackupFilesTests)
#include "LinuxBackupFilesTests.moc"
