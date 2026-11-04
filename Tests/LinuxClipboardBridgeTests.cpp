#include "ClipboardBridge.h"
#include "TextConverter.h"
#include <QtTest>

class FakeClipboard : public ClipboardAccess {
public:
    QMimeData data;
    int reads = 0, writes = 0;
    bool unavailable = false;
    QString written;
    const QMimeData* mimeData() override { ++reads; return unavailable ? nullptr : &data; }
    void setText(const QString& text) override { ++writes; written = text; }
};
class LinuxClipboardBridgeTests : public QObject {
    Q_OBJECT
private slots:
    void missingGuiIsUnavailable() {
        ClipboardBridge bridge;
        QCOMPARE(bridge.readText()["status"].toString(), "unavailable");
        QCOMPARE(bridge.copyText("valid"), "unavailable");
    }
    void explicitOnlyAndUnicode() {
        FakeClipboard fake;
        fake.data.setText(QStringLiteral("Tiếng Việt 😀\n<b>plain</b>"));
        ClipboardBridge bridge(&fake);
        QCOMPARE(fake.reads + fake.writes, 0);
        const auto reply = bridge.readText();
        QVERIFY(reply["ok"].toBool());
        QCOMPARE(reply["text"].toString(), fake.data.text());
        QCOMPARE(fake.reads, 1);
        QCOMPARE(fake.writes, 0);
        QCOMPARE(bridge.copyText(reply["text"].toString()), "copied");
        QCOMPARE(fake.written, fake.data.text());
        QCOMPARE(fake.reads, 1); // Copy does not read back unrelated clipboard data.
        QCOMPARE(fake.writes, 1);
    }
    void rejectedPaste_data() {
        QTest::addColumn<QString>("kind");
        QTest::addColumn<QString>("status");
        QTest::newRow("unavailable") << QString("unavailable") << QString("unavailable");
        QTest::newRow("html") << QString("html") << QString("not-text");
        QTest::newRow("empty") << QString("empty") << QString("empty");
        QTest::newRow("large") << QString("large") << QString("too-large");
        QTest::newRow("nul") << QString("nul") << QString("invalid");
        QTest::newRow("surrogate") << QString("surrogate") << QString("invalid");
    }
    void rejectedPaste() {
        QFETCH(QString, kind);
        QFETCH(QString, status);
        FakeClipboard fake;
        if (kind == "unavailable") fake.unavailable = true;
        else if (kind == "html") fake.data.setHtml("<b>not plain text</b>");
        else if (kind == "empty") fake.data.setText("");
        else if (kind == "large") fake.data.setText(QString(TextConverter::inputLimit + 1, QLatin1Char('a')));
        else if (kind == "nul") fake.data.setText(QString(QChar(0)));
        else fake.data.setText(QString(QChar(0xd800)));
        ClipboardBridge bridge(&fake);
        const auto reply = bridge.readText();
        QVERIFY(!reply["ok"].toBool());
        QCOMPARE(reply["status"].toString(), status);
        QVERIFY(!reply.contains("text"));
        QCOMPARE(fake.writes, 0);
    }
    void limitsAndInvalidCopy() {
        FakeClipboard fake;
        ClipboardBridge bridge(&fake);
        fake.data.setText(QStringLiteral("😀").repeated(TextConverter::inputLimit / 2));
        QVERIFY(bridge.readText()["ok"].toBool());
        const QString maximum(TextConverter::outputLimit, QLatin1Char('x'));
        QCOMPARE(bridge.copyText(maximum), "copied");
        QCOMPARE(fake.writes, 1);
        QCOMPARE(bridge.copyText(maximum + 'x'), "too-large");
        QCOMPARE(bridge.copyText(""), "empty");
        QCOMPARE(bridge.copyText(QString(QChar(0))), "invalid");
        QCOMPARE(bridge.copyText(QString(QChar(0xdc00))), "invalid");
        QCOMPARE(fake.writes, 1);
        QCOMPARE(fake.written, maximum); // Rejection never erases the user's clipboard.
    }
};
QTEST_APPLESS_MAIN(LinuxClipboardBridgeTests)
#include "LinuxClipboardBridgeTests.moc"
