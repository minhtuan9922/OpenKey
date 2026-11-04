#include "UiEnvironment.h"
#include <QStringList>
#include <QtTest>

class LinuxUiEnvironmentTests : public QObject {
    Q_OBJECT
private slots:
    void snapshot_data() {
        QTest::addColumn<QString>("session");
        QTest::addColumn<QString>("platform");
        QTest::addColumn<QString>("sessionType");
        QTest::addColumn<QString>("platformName");

        const QStringList sessions = {"x11", "wayland", "tty"};
        const QStringList platforms = {"xcb", "wayland", "wayland-egl", "offscreen",
            "minimal", "vnc", "eglfs", "linuxfb"};
        const QStringList badText = {QString(), QStringLiteral(""), "unknown", "badtext",
            "<b>wayland</b>", "<script>alert(1)</script>", "wayland\nxcb", "\n", "\r\n",
            "Wayland", "WAYLAND", " wayland", "wayland ", "\twayland", "wayland\n",
            QString::fromLatin1("wayland\0", 8)};

        // Cross every input with every other input to catch inference and coupling,
        // including wayland/xcb, tty/offscreen, and one invalid value at a time.
        QStringList sessionInputs = sessions + badText;
        QStringList platformInputs = platforms + badText;
        for (const auto& value : platforms) {
            if (value != QStringLiteral("wayland")) sessionInputs.append(value);
        }
        platformInputs.append({"x11", "tty"});

        // Every accepted token must reject changes in case or surrounding whitespace.
        for (const auto& value : sessions)
            sessionInputs.append({value.toUpper(), " " + value, value + " ", value + "\n"});
        for (const auto& value : platforms)
            platformInputs.append({value.toUpper(), " " + value, value + " ", value + "\n"});

        for (int s = 0; s < sessionInputs.size(); ++s) {
            for (int p = 0; p < platformInputs.size(); ++p) {
                const auto name = QStringLiteral("session-%1-platform-%2").arg(s).arg(p).toLatin1();
                // Only the initial entries are whitelisted; all appended inputs are invalid.
                QTest::newRow(name.constData()) << sessionInputs[s] << platformInputs[p]
                    << (s < sessions.size() ? sessions[s] : QStringLiteral("unknown"))
                    << (p < platforms.size() ? platforms[p] : QStringLiteral("unknown"));
            }
        }
    }

    void snapshot() {
        QFETCH(QString, session);
        QFETCH(QString, platform);
        QFETCH(QString, sessionType);
        QFETCH(QString, platformName);

        const QVariantMap expected = {{QStringLiteral("sessionType"), sessionType},
                                     {QStringLiteral("platformName"), platformName}};
        QCOMPARE(UiEnvironment::snapshot(session, platform), expected);
    }
};

QTEST_APPLESS_MAIN(LinuxUiEnvironmentTests)
#include "LinuxUiEnvironmentTests.moc"
