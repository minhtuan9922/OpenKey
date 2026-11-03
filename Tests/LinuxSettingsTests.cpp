#include "SettingsOverview.h"
#include <QGuiApplication>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QtTest>
#include <memory>
#include <cmath>

// UI interaction double; no D-Bus or config writes, real controller tested separately.
class StyleControllerDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString runtimeStyle MEMBER style NOTIFY changed)
    Q_PROPERTY(QString result MEMBER result NOTIFY changed)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY changed)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
public:
    QString style = "Preedit", result = "loaded";
    bool ready = true, busy = false;
    int applies = 0, reloads = 0;
    Q_INVOKABLE void apply(const QString& value) {
        ++applies;
        style = value;
        result = "confirmed";
        emit changed();
    }
    Q_INVOKABLE void reload() { ++reloads; emit changed(); }
signals:
    void changed();
};

class LinuxSettingsTests : public QObject {
    Q_OBJECT
    std::unique_ptr<QQmlApplicationEngine> engine;
    QTemporaryDir directory;
    std::unique_ptr<SettingsOverview> model;
    QQuickWindow* window = nullptr;
    QStringList warnings;
    QQuickItem* findItem(QQuickItem* parent, const QString& name) {
        if (parent->objectName() == name) return parent;
        for (auto* child : parent->childItems()) {
            if (auto* found = findItem(child, name)) return found;
        }
        return nullptr;
    }
    QQuickItem* item(const char* name) {
        return findItem(window->contentItem(), QString::fromLatin1(name));
    }
    void click(const char* name) {
        auto* target = item(name);
        QVERIFY2(target, name);
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier,
            target->mapToScene(QPointF(target->width()/2, target->height()/2)).toPoint());
    }
private slots:
    void init() {
        warnings.clear();
        QVERIFY(directory.isValid());
        model = std::make_unique<SettingsOverview>(directory.path(), directory.path() + "/config");
        engine = std::make_unique<QQmlApplicationEngine>();
        engine->setInitialProperties({{"overviewModel", QVariant::fromValue(model.get())}});
        connect(engine.get(), &QQmlApplicationEngine::warnings, this,
            [this](const QList<QQmlError>& errors) {
                for (const auto& error : errors) warnings.append(error.toString());
            });
        engine->load(QUrl::fromLocalFile(QStringLiteral(OPENKEY_SETTINGS_QML)));
        QVERIFY2(!engine->rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        window = qobject_cast<QQuickWindow*>(engine->rootObjects().first());
        QVERIFY(window);
        QTRY_VERIFY(window->isVisible());
        QTest::qWait(50);
    }
    void cleanup() {
        if (window) window->close();
        engine.reset();
        model.reset();
        window = nullptr;
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
    void navigation() {
        const QStringList keys{"overview", "spelling", "hotkeys", "compatibility",
                               "advanced", "macro", "convert", "about"};
        for (int i = 0; i < keys.size(); ++i) {
            click(qPrintable("nav-" + keys[i]));
            QCOMPARE(window->property("pageIndex").toInt(), i);
            QVERIFY(item("pageTitle"));
            QVERIFY(!item("pageTitle")->property("text").toString().isEmpty());
        }
        QVariant result;
        QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Q_RETURN_ARG(QVariant, result),
                                         Q_ARG(QVariant, -1)));
        QVERIFY(!result.toBool());
        QCOMPARE(window->property("pageIndex").toInt(), 7);
        QVERIFY(QMetaObject::invokeMethod(window, "selectPage", Q_RETURN_ARG(QVariant, result),
                                         Q_ARG(QVariant, 8)));
        QVERIFY(!result.toBool());
        QCOMPARE(window->property("pageIndex").toInt(), 7);
    }
    void collapse() {
        click("nav-macro");
        click("sidebarToggle");
        QTRY_COMPARE(item("sidebar")->width(), 72.0);
        QCOMPARE(window->property("pageIndex").toInt(), 5);
        click("nav-hotkeys");
        QCOMPARE(window->property("pageIndex").toInt(), 2);
        click("sidebarToggle");
        QTRY_COMPARE(item("sidebar")->width(), 244.0);
    }
    void themes() {
        auto* colors = window->findChild<QObject*>("uiPalette");
        QVERIFY(colors);
        window->setProperty("themeMode", "light");
        QCOMPARE(colors->property("dark").toBool(), false);
        const auto light = window->color();
        window->setProperty("themeMode", "dark");
        QCOMPARE(colors->property("dark").toBool(), true);
        QVERIFY(window->color() != light);
        auto* picker = item("themePicker");
        QVERIFY(picker);
        QCOMPARE(picker->property("currentIndex").toInt(), 2);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        QTRY_COMPARE(window->property("themeMode").toString(), QString("system"));
        QTest::keyClick(window, Qt::Key_End);
        QTRY_COMPARE(window->property("themeMode").toString(), QString("dark"));
    }
    void keyboardNavigation() {
        auto* overview = item("nav-overview");
        QVERIFY(overview);
        overview->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(window->property("pageIndex").toInt(), 1);
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(window->property("pageIndex").toInt(), 0);
        auto* about = item("nav-about");
        QVERIFY(about);
        about->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(window->property("pageIndex").toInt(), 6);
    }
    void smallWindow() {
        window->resize(720, 560);
        QTest::qWait(100);
        auto* scroll = item("contentScroll");
        QVERIFY(scroll);
        QVERIFY(scroll->width() > 300);
        const auto right = scroll->mapToScene(QPointF(scroll->width(), 0)).x();
        QVERIFY(right <= window->width());
        QVERIFY(!window->grabWindow().isNull());
        click("nav-compatibility");
        QCOMPARE(window->property("pageIndex").toInt(), 3);
    }
    void readOnlyOverview() {
        QFile file(directory.path() + "/config");
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        const QByteArray content("InputStyle=Direct\nDirectDiagnostics=False\n");
        QCOMPARE(file.write(content), content.size());
        file.close();
        model->refresh();
        QTRY_COMPARE(window->property("savedStyle").toString(), QString("direct"));
        QCOMPARE(window->property("serviceState").toString(), "unavailable");
        QCOMPARE(window->property("sampledInput").toString(), "unknown");
        QCOMPARE(window->property("configState").toString(), "read");
        QVERIFY(item("savedConfigCard")->property("description").toString().contains("Direct"));
        QVERIFY(item("inputStatusCard")->property("description").toString().contains("Không lấy được"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), content);
    }
    void inputStyleControls() {
        auto* picker = item("inputStylePicker");
        auto* apply = item("applyInputStyle");
        auto* reload = item("reloadInputStyle");
        QVERIFY(picker && apply && reload);
        QVERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!reload->isEnabled());
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        QTRY_VERIFY(picker->isEnabled());
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 0);
        QVERIFY(!apply->isEnabled());
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.applies, 0); // Selecting does not apply.
        QTRY_VERIFY(apply->isEnabled());
        click("applyInputStyle");
        QCOMPARE(controller.applies, 1);
        QCOMPARE(controller.style, "Direct");
        QTRY_VERIFY(!apply->isEnabled());
        QVERIFY(item("inputStyleFeedback")->property("text").toString().contains("runtime"));
        click("reloadInputStyle");
        QCOMPARE(controller.reloads, 1);
        controller.busy = true;
        emit controller.changed();
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!reload->isEnabled());
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void popupContrast_data() {
        QTest::addColumn<QString>("theme");
        QTest::addColumn<QString>("pickerName");
        for (const auto& theme : {"light", "dark"}) {
            for (const auto& pickerName : {"inputStylePicker", "themePicker"}) {
                const QByteArray name = QByteArray(theme) + '-' + pickerName;
                QTest::newRow(name.constData()) << QString(theme) << QString(pickerName);
            }
        }
    }
    void popupContrast() {
        QFETCH(QString, theme);
        QFETCH(QString, pickerName);
        StyleControllerDouble controller;
        controller.style = "Direct"; // Reproduce the selected row in the report.
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("themeMode", theme);
        auto* picker = item(qPrintable(pickerName));
        QVERIFY(picker);
        click(qPrintable(pickerName));
        auto* popup = picker->property("popup").value<QObject*>();
        QVERIFY(popup);
        QTRY_VERIFY(popup->property("visible").toBool());
        const int count = picker->property("count").toInt();
        const auto luminance = [](const QColor& color) {
            const auto linear = [](double channel) {
                return channel <= 0.04045 ? channel / 12.92 : std::pow((channel + 0.055) / 1.055, 2.4);
            };
            return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF()) + 0.0722 * linear(color.blueF());
        };
        for (int step = 0; step < count; ++step) {
            for (int row = 0; row < count; ++row) {
                const QString prefix = pickerName + "-option-";
                QTRY_VERIFY(item(qPrintable(prefix + "text-" + QString::number(row))));
                auto* label = item(qPrintable(prefix + "text-" + QString::number(row)));
                auto* background = item(qPrintable(prefix + "background-" + QString::number(row)));
                QVERIFY(background);
                const auto foregroundColor = label->property("color").value<QColor>();
                const auto backgroundColor = background->property("color").value<QColor>();
                const auto a = luminance(foregroundColor), b = luminance(backgroundColor);
                const auto ratio = (std::max(a, b) + 0.05) / (std::min(a, b) + 0.05);
                QVERIFY2(ratio >= 4.5, qPrintable(QString("%1 row %2 contrast %3").arg(pickerName).arg(row).arg(ratio)));
            }
            QTest::keyClick(window, Qt::Key_Down);
        }
        const auto screenshot = window->grabWindow();
        QVERIFY(!screenshot.isNull());
        const auto screenshotDirectory = qEnvironmentVariable("OPENKEY_UI_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            const auto path = QDir(screenshotDirectory).filePath(theme + '-' + pickerName + ".png");
            QVERIFY(!QFile::exists(path));
            QVERIFY(screenshot.save(path));
        }
        QTest::keyClick(window, Qt::Key_Escape);
        QTRY_VERIFY(!popup->property("visible").toBool());
        QCOMPARE(controller.applies, 0); // Opening/navigating a popup must not apply.
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    LinuxSettingsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "LinuxSettingsTests.moc"
