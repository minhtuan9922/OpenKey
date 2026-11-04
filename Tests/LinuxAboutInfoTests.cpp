#include "AboutInfo.h"
#include <QFile>
#include <QSignalSpy>
#include <QtTest>

class LinuxAboutInfoTests : public QObject {
    Q_OBJECT
private slots:
    void buildMetadata() {
        AboutInfo info;
        QCOMPARE(info.sourceVersion(), QStringLiteral(OPENKEY_EXPECTED_UI_VERSION));
        QCOMPARE(info.qtBuildVersion(), QStringLiteral(QT_VERSION_STR));
        QCOMPARE(info.qtRuntimeVersion(), QString::fromLatin1(qVersion()));
        QCOMPARE(info.licenseState(), "idle");
        QVERIFY(info.licenseText().isEmpty());
    }
    void embeddedLicenseAndReset() {
        QFile original(QStringLiteral(OPENKEY_LICENSE_SOURCE));
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto expected = QString::fromUtf8(original.readAll());
        QVERIFY(expected.contains("GNU GENERAL PUBLIC LICENSE"));
        AboutInfo first, second;
        QSignalSpy changes(&first, &AboutInfo::changed);
        first.loadLicense();
        QCOMPARE(first.licenseState(), "ready");
        QCOMPARE(first.licenseText(), expected);
        QCOMPARE(changes.size(), 1);
        first.loadLicense();
        QCOMPARE(changes.size(), 1); // Showing an already loaded resource is idempotent.
        QVERIFY(second.licenseText().isEmpty());
        first.clearLicense();
        QCOMPARE(first.licenseState(), "idle");
        QVERIFY(first.licenseText().isEmpty());
        first.loadLicense();
        QCOMPARE(first.licenseText(), expected);
        second.loadLicense();
        first.clearLicense();
        QCOMPARE(second.licenseText(), expected);
    }
};
QTEST_APPLESS_MAIN(LinuxAboutInfoTests)
#include "LinuxAboutInfoTests.moc"
