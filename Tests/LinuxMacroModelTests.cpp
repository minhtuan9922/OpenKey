#include "MacroModel.h"
#include "../Sources/OpenKey/linux/MacroTable.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class MacroTests : public QObject {
    Q_OBJECT
private slots:
    void applicationRulesValidation() {
        MacroModel rules(nullptr, true);
        QVERIFY(rules.put("gtk3-im:google-chrome", "Preedit"));
        QVERIFY(rules.put("/usr/bin/code", "NativeDirect"));
        const auto before = rules.encoded();
        for (const auto& id : {"gnome-shell", "unknown", "/usr/bin/gnome-shell", "gtk3-im:gnome-shell", "Chrome title"})
            QVERIFY(!rules.put(id, "Direct"));
        QVERIFY(!rules.put("code", "anything"));
        QCOMPARE(rules.encoded(), before);
        MacroModel copy(nullptr, true);
        QVERIFY(copy.importJson(rules.exportJson()));
        QCOMPARE(copy.encoded(), before);
        MacroModel macros;
        QVERIFY(!macros.importJson(rules.exportJson()));
        QVERIFY(!rules.importJson(macros.exportJson()));
    }
    void roundTrip() {
        MacroModel model;
        QVERIFY(model.put("vn", "Việt Nam 😀\nxin chào"));
        QVERIFY(model.put("đc", "địa chỉ"));
        const auto encoded = model.encoded(), json = model.exportJson();
        MacroModel copy;
        QVERIFY(copy.importJson(json));
        QCOMPARE(copy.encoded(), encoded);
        QVERIFY(copy.put("vn", "Việt Nam"));
        QCOMPARE(copy.entries().size(), 2);
        QVERIFY(copy.remove("vn"));
        QVERIFY(!copy.remove("vn"));
        QVERIFY(model.load(""));
        QVERIFY(model.entries().isEmpty());
    }
    void invalidImport_data() {
        QTest::addColumn<QString>("json");
        for (const auto& value : {QStringLiteral("{}"), QStringLiteral("[]"), QStringLiteral("bad"),
             QStringLiteral("{\"format\":\"openkey-ui\",\"version\":1,\"entries\":[]}"),
             QStringLiteral("{\"format\":\"openkey-macros\",\"version\":1,\"entries\":[{\"key\":\"x\",\"value\":\"a\"},{\"key\":\"x\",\"value\":\"b\"}]}"),
             QStringLiteral("{\"format\":\"openkey-macros\",\"version\":true,\"entries\":[]}"), QString(131073, 'a')})
            QTest::newRow(qPrintable(QString::number(value.size()) + value.left(20))) << value;
    }
    void invalidImport() {
        QFETCH(QString, json);
        MacroModel model;
        QVERIFY(model.put("vn", "Việt Nam"));
        const auto before = model.encoded();
        QVERIFY(!model.importJson(json));
        QCOMPARE(model.encoded(), before);
    }
    void validationAndLimits() {
        MacroModel model;
        QVERIFY(!model.put("a b", "x"));
        QVERIFY(!model.put("", "x"));
        QVERIFY(!model.put("a", ""));
        QVERIFY(!model.put(QString(33, 'a'), "x"));
        QVERIFY(!model.put("a", QString(4097, 'x')));
        QVERIFY(!model.put("a", QString(QChar(0xd800))));
        QVERIFY(!model.put("a", QString(QChar(0))));
        for (int i = 0; i < 256; ++i) QVERIFY(model.put(QString::number(i), "x"));
        const auto before = model.encoded();
        QVERIFY(!model.put("extra", "x"));
        QCOMPARE(model.encoded(), before);
        for (const auto& malformed : {"a:b;", "ff:61;", "61:00;", "61:62", "61:62;61:63;", "61:ff;"}) {
            QVERIFY(!model.load(malformed));
            QCOMPARE(model.encoded(), before);
        }
        MacroModel large;
        for (int i = 0; i < 7; ++i) QVERIFY(large.put(QString::number(i), QString(4096, 'x')));
        QVERIFY(!large.put("8", QString(4096, 'x')));
    }
};
QTEST_GUILESS_MAIN(MacroTests)
#include "LinuxMacroModelTests.moc"
