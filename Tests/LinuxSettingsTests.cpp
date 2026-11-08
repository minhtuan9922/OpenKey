#include "SettingsOverview.h"
#include "UiPreferences.h"
#include "TextConverter.h"
#include "AboutInfo.h"
#include "ClipboardBridge.h"
#include "InputConfigBackup.h"
#include "BackupFiles.h"
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQmlProperty>
#include <QQuickItem>
#include "MacroModel.h"
#include "InputMethodController.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSemaphore>
#include <QThreadPool>
#include <QRunnable>
#include <QtTest>
#include <memory>
#include <cmath>
#include <algorithm>

// UI interaction double; no D-Bus or config writes, real controller tested separately.
class StyleControllerDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString runtimeStyle MEMBER style NOTIFY changed)
    Q_PROPERTY(bool commitOnlySupported MEMBER supportsCommitOnly NOTIFY changed)
    Q_PROPERTY(bool nativeDirectSupported MEMBER supportsNativeDirect NOTIFY changed)
    Q_PROPERTY(QString runtimeDiagnostics MEMBER diagnosticsValue NOTIFY changed)
    Q_PROPERTY(bool spellingSupported MEMBER supportsSpelling NOTIFY changed)
    Q_PROPERTY(QString runtimeCheckSpelling MEMBER checkSpelling NOTIFY changed)
    Q_PROPERTY(QString runtimeModernOrthography MEMBER modernOrthography NOTIFY changed)
    Q_PROPERTY(QVariantMap runtimeAdvanced MEMBER advanced NOTIFY changed)
    Q_PROPERTY(bool applicationRulesSupported MEMBER supportsRules NOTIFY changed)
    Q_PROPERTY(QString runtimeApplicationRules MEMBER appRules NOTIFY changed)
    Q_PROPERTY(bool macrosSupported MEMBER supportsMacros NOTIFY changed)
    Q_PROPERTY(QString runtimeMacrosEnabled MEMBER macrosEnabled NOTIFY changed)
    Q_PROPERTY(QString runtimeMacroTable MEMBER macroTable NOTIFY changed)
    Q_PROPERTY(bool quickTelexSupported MEMBER supportsQuickTelex NOTIFY changed)
    Q_PROPERTY(QString runtimeQuickTelex MEMBER quickTelex NOTIFY changed)
    Q_PROPERTY(QString result MEMBER result NOTIFY changed)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY changed)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
public:
    QString style = "Preedit", result = "loaded";
    QString diagnosticsValue = "False";
    QString checkSpelling = "True", modernOrthography = "True";
    bool supportsSpelling = true;
    bool supportsCommitOnly = false;
    bool supportsNativeDirect = false;
    QVariantMap advanced{{"FreeMark", "False"}, {"AllowConsonantZFWJ", "False"}, {"QuickStartConsonant", "False"}, {"QuickEndConsonant", "False"}, {"RestoreWrongSpelling", "False"}, {"RememberInputMethodPerApplication", "False"}};
    Q_INVOKABLE void applyAdvanced(const QString& key, const QString& value) { advanced[key] = value; emit changed(); }
    bool supportsRules = true;
    QString appRules;
    Q_INVOKABLE void applyApplicationRules(const QString& value) { appRules = value; emit changed(); }
    bool supportsMacros = true;
    QString macrosEnabled = "False", macroTable;
    Q_INVOKABLE void applyMacrosEnabled(const QString& value) { macrosEnabled = value; emit changed(); }
    Q_INVOKABLE void applyMacroTable(const QString& value) { macroTable = value; emit changed(); }
    bool supportsQuickTelex = true;
    QString quickTelex = "False";
    int quickTelexApplies = 0;
    bool ready = true, busy = false;
    int applies = 0, reloads = 0;
    int diagnosticsApplies = 0;
    int spellingApplies = 0, modernApplies = 0;
    int backupInspections = 0, backupRestores = 0;
    QVariantMap backupRuntime() const {
        return {{"InputStyle", style}, {"DirectDiagnostics", diagnosticsValue},
            {"CommitOnlySupported", supportsCommitOnly}, {"NativeDirectSupported", supportsNativeDirect}};
    }
    Q_INVOKABLE QString backupJson() const { return InputConfigBackup::encode(backupRuntime()); }
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json) {
        ++backupInspections;
        QVariantMap patch;
        const bool ok = InputConfigBackup::decode(json, patch) && InputConfigBackup::supported(patch, backupRuntime());
        return {{"ok", ok}, {"settings", patch}, {"current", backupRuntime()}};
    }
    Q_INVOKABLE void restoreBackup(const QString& json) {
        ++backupRestores;
        QVariantMap patch;
        if (!InputConfigBackup::decode(json, patch)) return;
        style = patch.value("InputStyle").toString();
        diagnosticsValue = patch.value("DirectDiagnostics").toString();
        result = "confirmed";
        emit changed();
    }
    Q_INVOKABLE void apply(const QString& value) {
        ++applies;
        style = value;
        result = "confirmed";
        emit changed();
    }
    Q_INVOKABLE void reload() { ++reloads; emit changed(); }
    Q_INVOKABLE void applyDiagnostics(const QString& value) {
        ++diagnosticsApplies;
        diagnosticsValue = value;
        result = "confirmed";
        emit changed();
    }
    Q_INVOKABLE void applySpelling(const QString& value) {
        ++spellingApplies; checkSpelling = value; result = "confirmed"; emit changed();
    }
    Q_INVOKABLE void applyModernOrthography(const QString& value) {
        ++modernApplies; modernOrthography = value; result = "confirmed"; emit changed();
    }
    Q_INVOKABLE void applyQuickTelex(const QString& value) {
        ++quickTelexApplies; quickTelex = value; result = "confirmed"; emit changed();
    }
signals:
    void changed();
};

class InputMethodsDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current MEMBER current NOTIFY changed)
    Q_PROPERTY(QStringList methods MEMBER methods NOTIFY changed)
    Q_PROPERTY(bool active MEMBER active NOTIFY changed)
    Q_PROPERTY(bool ready MEMBER ready NOTIFY changed)
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
    Q_PROPERTY(QString result MEMBER result NOTIFY changed)
public:
    QString current = "openkey-telex", result = "loaded";
    QStringList methods{"keyboard-us", "openkey-telex", "openkey-vni", "openkey-simple-telex1", "openkey-simple-telex2"};
    bool active = true, ready = true, busy = false;
    int reloads = 0, selections = 0;
    Q_INVOKABLE QString backupJson() const { return QString::fromUtf8(QJsonDocument(QJsonObject{{"format", "openkey-input-method"}, {"version", 1}, {"method", current}}).toJson()); }
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json) const { const auto method = InputMethodController::methodFromBackup(json); return {{"ok", !method.isEmpty()}, {"method", method}}; }
    Q_INVOKABLE void reload() { ++reloads; emit changed(); }
    Q_INVOKABLE void select(const QString& method) { ++selections; current = method; active = true; result = "confirmed"; emit changed(); }
signals:
    void changed();
};

class FileConversionDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy MEMBER busy NOTIFY changed)
    Q_PROPERTY(QString state MEMBER state NOTIFY changed)
public:
    bool busy = false;
    QString state = "idle", source, target;
    int starts = 0, cancels = 0;
    Q_INVOKABLE void start(const QUrl&, const QUrl&, const QString& from, const QString& to) { ++starts; source = from; target = to; busy = true; state = "running"; emit changed(); }
    Q_INVOKABLE void cancel() { ++cancels; busy = false; state = "cancelled"; emit changed(); }
signals:
    void changed();
};

class HotkeyOverviewDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state MEMBER state NOTIFY changed)
    Q_PROPERTY(QVariantMap shortcuts MEMBER shortcuts NOTIFY changed)
public:
    QString state = "ready";
    QVariantMap shortcuts;
    int reloads = 0, writes = 0;
    Q_INVOKABLE void apply(const QString& field, const QString& keys) { ++writes; shortcuts[field] = keys.split('\n'); emit changed(); }
    Q_INVOKABLE QString backupJson() { return "{}"; }
    Q_INVOKABLE QVariantMap inspectBackup(const QString&) { return {{"ok", false}}; }
    Q_INVOKABLE void reload() { ++reloads; emit changed(); }
signals:
    void changed();
};

class StatusOverviewDouble : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString inputStatus MEMBER input NOTIFY changed)
    Q_PROPERTY(QString serviceState MEMBER service NOTIFY changed)
    Q_PROPERTY(QString configState MEMBER config NOTIFY changed)
    Q_PROPERTY(QString inputStyle MEMBER style NOTIFY changed)
    Q_PROPERTY(QString diagnostics MEMBER diagnostics NOTIFY changed)
public:
    QString input = "telex", service = "connected", config = "read";
    QString style = "preedit", diagnostics = "disabled";
    int refreshes = 0;
    Q_INVOKABLE void refresh() { ++refreshes; emit changed(); }
signals:
    void changed();
};

class UiClipboard : public ClipboardAccess {
public:
    QMimeData data;
    int reads = 0, writes = 0;
    QString written;
    const QMimeData* mimeData() override { ++reads; return &data; }
    void setText(const QString& text) override { ++writes; written = text; }
};

// Hold the worker pool so busy/cancel UI tests never depend on CPU timing.
class ConversionGate {
    struct State { QSemaphore entered, release; };
    std::shared_ptr<State> state = std::make_shared<State>();
    QThreadPool* pool = QThreadPool::globalInstance();
    int previous = pool->maxThreadCount();
    bool released = false;
public:
    ConversionGate() {
        pool->setMaxThreadCount(1);
        pool->start(QRunnable::create([gate = state] {
            gate->entered.release();
            gate->release.acquire();
        }));
    }
    bool entered() { return state->entered.tryAcquire(1, 1000); }
    void release() { if (!released) { released = true; state->release.release(); } }
    ~ConversionGate() { release(); pool->waitForDone(5000); pool->setMaxThreadCount(previous); }
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
    void reveal(QQuickItem* target) {
        // Page visibility changes schedule a layout polish. Scroll using its
        // final geometry rather than the position from the previous page.
        QTest::qWait(30);
        auto* scroll = item("contentScroll");
        QVERIFY(scroll && target);
        auto* flickable = scroll->property("contentItem").value<QObject*>();
        QVERIFY(flickable);
        const auto delta = target->mapToScene(QPointF(0, target->height()/2)).y() -
            scroll->mapToScene(QPointF(0, scroll->height()/2)).y();
        const auto maxY = std::max(0.0, flickable->property("contentHeight").toDouble() - scroll->height());
        const auto nextY = std::clamp(flickable->property("contentY").toDouble() + delta, 0.0, maxY);
        QVERIFY(flickable->setProperty("contentY", nextY));
        QTest::qWait(30);
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
    void windowNavigationShortcuts_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void windowNavigationShortcuts() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        StyleControllerDouble controller;
        UiClipboard clipboard;
        ClipboardBridge bridge(&clipboard);
        TextConverter converter;
        AboutInfo about;
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("clipboardBridge", QVariant::fromValue(&bridge));
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        window->setProperty("aboutInfo", QVariant::fromValue(&about));
        window->requestActivate();
        click("nav-convert");
        item("conversionInput")->setProperty("text", QStringLiteral("Bản nháp không bị mất"));
        item("conversionInput")->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 7);
        QVERIFY(item("nav-about")->hasActiveFocus());
        QCOMPARE(about.licenseState(), "idle");
        QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 0);
        QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 7);
        QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier | Qt::ShiftModifier);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 6);
        QCOMPARE(item("conversionInput")->property("text").toString(), "Bản nháp không bị mất");
        auto* last = item("conversionOutput");
        reveal(last);
        auto* flickable = item("contentScroll")->property("contentItem").value<QObject*>();
        QVERIFY(flickable);
        QVERIFY(flickable->property("contentY").toDouble() > 0);
        QTest::keyClick(window, Qt::Key_F1);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 7);
        QTRY_COMPARE(flickable->property("contentY").toDouble(), 0.0);
        QCOMPARE(controller.applies + controller.diagnosticsApplies + controller.spellingApplies +
            controller.modernApplies + controller.quickTelexApplies + controller.reloads, 0);
        QCOMPARE(clipboard.reads + clipboard.writes, 0);
        QCOMPARE(converter.state(), "idle");
        QCOMPARE(about.licenseState(), "idle");
        click("sidebarToggle");
        auto* toggle = item("sidebarToggle");
        toggle->forceActiveFocus(Qt::TabFocusReason);
        QTRY_VERIFY(toggle->hasActiveFocus());
        auto* background = toggle->property("background").value<QObject*>();
        QVERIFY(background);
        QTRY_COMPARE(QQmlProperty(background, "border.width").read().toInt(), 2);
        QTest::keyClick(window, Qt::Key_Tab);
        QTRY_VERIFY(!toggle->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab, Qt::ShiftModifier);
        QTRY_VERIFY(toggle->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Tab, Qt::ControlModifier);
        QTRY_COMPARE(window->property("pageIndex").toInt(), 0);
        QVERIFY(item("nav-overview")->hasActiveFocus());
        click("nav-convert");
        QQuickWindow other;
        other.show();
        other.requestActivate();
        QTRY_VERIFY(other.isActive());
        QTest::keyClick(&other, Qt::Key_F1);
        QTest::keyClick(&other, Qt::Key_Tab, Qt::ControlModifier);
        QCOMPARE(window->property("pageIndex").toInt(), 6); // Not an application/global shortcut.
        other.close();
        window->requestActivate();
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("aboutInfo", QVariant::fromValue<QObject*>(nullptr));
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
    void runtimeStatus_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void runtimeStatus() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        auto* summary = item("runtimeStatusSummary");
        auto* details = item("runtimeStatusDetails");
        auto* guidance = item("runtimeStatusGuidance");
        auto* refresh = item("refreshRuntimeStatus");
        QVERIFY(summary && details && guidance && refresh);
        QVERIFY(!refresh->isEnabled());
        QVERIFY(summary->property("text").toString().contains("Chưa đủ nguồn"));
        StatusOverviewDouble overview;
        StyleControllerDouble controller;
        UiClipboard clipboard;
        ClipboardBridge bridge(&clipboard);
        TextConverter converter;
        window->setProperty("overviewModel", QVariant::fromValue(&overview));
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("clipboardBridge", QVariant::fromValue(&bridge));
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        QTRY_VERIFY(summary->property("text").toString().contains("Có dữ liệu"));
        QVERIFY(details->property("text").toString().contains("OpenKey Telex"));
        QVERIFY(details->property("text").toString().contains("không chứng minh runtime"));
        QCOMPARE(overview.refreshes + controller.reloads, 0);
        click("nav-about");
        click("nav-overview");
        QCOMPARE(overview.refreshes + controller.reloads, 0); // Navigation never retries a failed write.
        reveal(refresh);
        refresh->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(overview.refreshes, 1);
        QCOMPARE(controller.reloads, 1);
        for (const auto& result : {QString("unconfirmed"), QString("conflict"), QString("unsupported"),
                                  QString("unavailable"), QString("error"), QString("invalid")}) {
            controller.result = result;
            controller.ready = false;
            emit controller.changed();
            QVERIFY(summary->property("text").toString().contains("cần kiểm tra"));
            QVERIFY(refresh->isEnabled());
            if (result == "unconfirmed") {
                QVERIFY(guidance->property("text").toString().contains("không tự Apply lại"));
                QVERIFY(details->property("text").toString().contains("có thể đã được áp dụng"));
            }
        }
        controller.result = "applying";
        controller.busy = true;
        emit controller.changed();
        QVERIFY(!refresh->isEnabled());
        QVERIFY(summary->property("text").toString().contains("Đang đọc / áp dụng"));
        controller.busy = false;
        controller.result = "confirmed";
        controller.ready = true;
        emit controller.changed();
        QVERIFY(summary->property("text").toString().contains("Có dữ liệu"));
        for (const auto& state : {QString("unavailable"), QString("timeout"), QString("protocol-error")}) {
            overview.service = state;
            emit overview.changed();
            QVERIFY(summary->property("text").toString().contains("cần kiểm tra"));
            QVERIFY(!details->property("text").toString().contains("OpenKey Telex"));
            if (state == "unavailable")
                QVERIFY(details->property("text").toString().contains("Không đồng nghĩa bộ gõ đã dừng"));
        }
        overview.service = "connected";
        overview.input = "unavailable";
        emit overview.changed();
        QVERIFY(summary->property("text").toString().contains("cần kiểm tra"));
        overview.input = "unknown";
        emit overview.changed();
        QVERIFY(summary->property("text").toString().contains("Chưa xác nhận"));
        overview.input = "vni";
        for (const auto& state : {QString("invalid"), QString("unreadable"), QString("missing"), QString("not-read")}) {
            overview.config = state;
            emit overview.changed();
            QVERIFY(!summary->property("text").toString().contains("Có dữ liệu"));
        }
        const auto unknown = QStringLiteral("<b>private-enum-value</b>");
        overview.service = overview.input = overview.config = unknown;
        controller.result = unknown;
        emit overview.changed();
        emit controller.changed();
        QVERIFY(summary->property("text").toString().contains("Chưa xác nhận"));
        QVERIFY(!details->property("text").toString().contains("private-enum-value"));
        QCOMPARE(details->property("textFormat").toInt(), 0); // Text.PlainText
        window->resize(720, 560);
        reveal(guidance);
        QVERIFY(guidance->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        QCOMPARE(controller.reloads, 1);
        QCOMPARE(overview.refreshes, 1);
        QCOMPARE(controller.applies + controller.diagnosticsApplies + controller.spellingApplies +
            controller.modernApplies + controller.quickTelexApplies, 0);
        QCOMPARE(clipboard.reads + clipboard.writes, 0);
        QCOMPARE(converter.state(), "idle");
        window->setProperty("overviewModel", QVariant::fromValue(model.get()));
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
        QVERIFY(!refresh->isEnabled());
    }
    void readOnlyHotkeys() {
        click("nav-hotkeys");
        auto* reload = item("reloadHotkeys");
        auto* trigger = item("hotkeyValue-TriggerKeys");
        auto* forward = item("hotkeyValue-EnumerateForwardKeys");
        auto* feedback = item("hotkeyFeedback");
        QVERIFY(reload && trigger && forward && feedback);
        QVERIFY(!reload->isEnabled());
        QCOMPARE(trigger->property("text").toString(), "Chưa xác định");
        QVERIFY(feedback->property("text").toString().contains("offline"));
        HotkeyOverviewDouble hotkeys;
        hotkeys.shortcuts.insert("TriggerKeys", QStringList{"Control+space", "Zenkaku_Hankaku"});
        hotkeys.shortcuts.insert("EnumerateForwardKeys", QStringList{});
        window->setProperty("hotkeyOverview", QVariant::fromValue(&hotkeys));
        QTRY_COMPARE(trigger->property("text").toString(), "Control+space / Zenkaku_Hankaku");
        QCOMPARE(forward->property("text").toString(), "Chưa gán");
        QCOMPARE(hotkeys.reloads, 0); // Opening the page does not invoke anything.
        QVERIFY(reload->isEnabled());
        reveal(reload);
        reload->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(hotkeys.reloads, 1);
        hotkeys.state = "loading";
        emit hotkeys.changed();
        QTRY_VERIFY(!reload->isEnabled());
        QCOMPARE(trigger->property("text").toString(), "Chưa xác định");
        hotkeys.state = "unavailable";
        emit hotkeys.changed();
        QTRY_VERIFY(reload->isEnabled());
        QCOMPARE(trigger->property("text").toString(), "Chưa xác định");
        QVERIFY(feedback->property("text").toString().contains("không tự khởi động"));
        hotkeys.state = "error";
        emit hotkeys.changed();
        QTRY_VERIFY(feedback->property("text").toString().contains("schema"));
        hotkeys.state = "ready";
        hotkeys.shortcuts.insert("TriggerKeys", QStringList{"Shift_L"});
        emit hotkeys.changed();
        QTRY_COMPARE(trigger->property("text").toString(), "Shift_L");
        window->resize(720, 560);
        QTest::qWait(50);
        auto* scroll = item("contentScroll");
        QVERIFY(trigger->width() <= scroll->width());
        QVERIFY(!window->grabWindow().isNull());
        window->setProperty("hotkeyOverview", QVariant::fromValue<QObject*>(nullptr));
        QTRY_VERIFY(!reload->isEnabled());
        QCOMPARE(trigger->property("text").toString(), "Chưa xác định");
    }
    void compatibilityControls() {
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("uiEnvironment", QVariantMap{{"sessionType", "wayland"}, {"platformName", "xcb"}});
        auto* picker = item("inputStylePicker");
        QVERIFY(picker);
        auto* originalPicker = picker;
        click("nav-compatibility");
        auto* environment = item("compatibilityEnvironmentCard");
        auto* direct = item("compatibilityDirectCard");
        QVERIFY(environment && direct);
        QVERIFY(environment->isVisible());
        const auto text = environment->property("description").toString();
        QVERIFY(text.contains("Wayland (XDG_SESSION_TYPE)"));
        QVERIFY(text.contains("xcb"));
        QVERIFY(text.contains("không xác định backend của VS Code"));
        QVERIFY(text.contains("Đã đọc schema/runtime"));
        QVERIFY(direct->property("description").toString().contains("không xác nhận Direct"));
        QCOMPARE(item("inputStylePicker"), originalPicker); // One shared editor, not two competing drafts.
        QCOMPARE(controller.applies, 0);
        QCOMPARE(controller.reloads, 0);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.applies, 0);
        auto* apply = item("applyInputStyle");
        QVERIFY(apply && apply->isEnabled());
        reveal(apply);
        click("applyInputStyle");
        QCOMPARE(controller.applies, 1);
        QCOMPARE(controller.style, "Direct");
        QCOMPARE(controller.diagnosticsApplies, 0);
        QCOMPARE(controller.spellingApplies, 0);
        QCOMPARE(controller.modernApplies, 0);
        click("nav-overview");
        QVERIFY(!environment->isVisible());
        QCOMPARE(item("inputStylePicker"), originalPicker);
        QCOMPARE(picker->property("currentIndex").toInt(), 1);
        click("nav-compatibility");
        controller.busy = true;
        controller.ready = false;
        emit controller.changed();
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QTRY_VERIFY(environment->property("description").toString().contains("Chưa xác nhận"));
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("uiEnvironment", QVariantMap{{"sessionType", "invalid"}, {"platformName", "invalid"}});
        QTRY_VERIFY(environment->property("description").toString().contains("Phiên đăng nhập: Chưa xác định"));
        QVERIFY(environment->property("description").toString().contains("Backend cửa sổ này: Chưa xác định"));
        QVERIFY(!picker->isEnabled());
        QVERIFY(!item("reloadInputStyle")->isEnabled());
        window->resize(720, 560);
        QTest::qWait(50);
        auto* last = item("compatibilityScopeCard");
        QVERIFY(last);
        reveal(last);
        QVERIFY(last->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
    }
    void typingTest_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void typingTest() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        auto* card = item("typingTestCard");
        auto* input = item("typingTestInput");
        auto* sample = item("typingTestSample");
        auto* clear = item("clearTypingTest");
        QVERIFY(card && input && sample && clear);
        QVERIFY(!card->isVisible());
        QVERIFY(input->property("text").toString().isEmpty());
        click("nav-compatibility");
        QVERIFY(card->isVisible());
        QVERIFY(!clear->isEnabled());
        auto* start = item("focusTypingTest");
        QVERIFY(start);
        reveal(start);
        click("focusTypingTest");
        QTRY_VERIFY(input->hasActiveFocus());
        for (char key : QByteArray("coongj")) QTest::keyClick(window, key);
        QCOMPARE(input->property("text").toString(), QString("coongj"));
        QTest::keyClick(window, Qt::Key_Backspace);
        QCOMPARE(input->property("text").toString(), QString("coong"));
        QTest::keyClick(window, Qt::Key_Return);
        for (char key : QByteArray("hoaf")) QTest::keyClick(window, key);
        QCOMPARE(input->property("text").toString(), QString("coong\nhoaf"));
        const QString draft = QString::fromUtf8("cộng hoà xã hội chủ nghĩa\nThử tiếp");
        input->setProperty("text", draft);
        reveal(sample);
        sample->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(sample->property("currentIndex").toInt(), 1);
        QVERIFY(item("typingTestInstructions")->property("text").toString().contains("co6ng5"));
        QCOMPARE(input->property("text").toString(), draft);
        click("nav-convert");
        QVERIFY(!card->isVisible());
        click("nav-compatibility");
        QCOMPARE(input->property("text").toString(), draft);
        // The test area works offline without enabling or applying a mode.
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        QVERIFY(input->isEnabled());
        window->resize(720, 560);
        reveal(clear);
        QVERIFY(card->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        click("clearTypingTest");
        QTRY_VERIFY(input->property("text").toString().isEmpty());
        QVERIFY(input->hasActiveFocus());
        QVERIFY(!clear->isEnabled());
        QCOMPARE(controller.applies, 0);
        QCOMPARE(controller.reloads, 0);
        QCOMPARE(controller.diagnosticsApplies, 0);
    }
    void legacyConversionControls() {
        TextConverter converter;
        UiClipboard fake;
        ClipboardBridge clipboard(&fake);
        window->setProperty("legacyConverter", QVariant::fromValue(&converter));
        window->setProperty("clipboardBridge", QVariant::fromValue(&clipboard));
        click("nav-convert");
        auto* input = item("legacyConversionSource");
        auto* output = item("legacyConversionResult");
        auto* convert = item("convertLegacyText");
        auto* copy = item("copyLegacyResult");
        QVERIFY(input && output && convert && copy);
        QVERIFY(!convert->isEnabled());
        QVERIFY(!copy->isEnabled());
        input->setProperty("text", QString::fromUtf8("tiếng việt"));
        QCOMPARE(converter.state(), QString("idle"));
        reveal(convert);
        click("convertLegacyText");
        QTRY_COMPARE(converter.state(), QString("converted"));
        QCOMPARE(output->property("text").toString(), QString::fromUtf8("tiÕng viÖt"));
        QVERIFY(output->property("readOnly").toBool());
        QCOMPARE(fake.reads + fake.writes, 0);
        reveal(copy);
        click("copyLegacyResult");
        QCOMPARE(fake.written, converter.result());
        QCOMPARE(fake.writes, 1);
        auto* target = item("legacyTargetEncoding");
        reveal(target);
        target->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        QCOMPARE(converter.state(), QString("idle"));
        QVERIFY(!copy->isEnabled());
        reveal(convert);
        click("convertLegacyText");
        QTRY_COMPARE(converter.state(), QString("converted"));
        QCOMPARE(converter.result(), QString::fromUtf8("tiếng việt"));
        click("nav-overview");
        click("nav-convert");
        QCOMPARE(input->property("text").toString(), QString::fromUtf8("tiếng việt"));
        window->resize(720, 560);
        reveal(item("clearLegacyConversion"));
        QVERIFY(item("legacyConversion")->width() <= item("contentScroll")->width());
        click("clearLegacyConversion");
        QVERIFY(input->property("text").toString().isEmpty());
        QCOMPARE(converter.state(), QString("idle"));
        QVERIFY(!copy->isEnabled());
        window->setProperty("legacyConverter", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
    }
    void unicodeConversion() {
        TextConverter converter;
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        click("nav-convert");
        auto* input = item("conversionInput");
        auto* output = item("conversionOutput");
        auto* convert = item("convertText");
        auto* clear = item("clearConversion");
        QVERIFY(input && output && convert && clear);
        QVERIFY(!convert->isEnabled());
        QCOMPARE(converter.state(), "idle");
        const QString source = QStringLiteral("Tiếng Việt — ĐẶNG 😀\nAbC");
        input->setProperty("text", source);
        QTRY_VERIFY(convert->isEnabled());
        QCOMPARE(converter.state(), "idle"); // Text entry never converts automatically.
        QCOMPARE(output->property("text").toString(), "");
        reveal(convert);
        click("convertText");
        QTRY_COMPARE(converter.state(), "converted");
        QCOMPARE(output->property("text").toString(), source);
        QCOMPARE(input->property("text").toString(), source);
        QVERIFY(output->property("readOnly").toBool());
        auto* normalization = item("conversionNormalization");
        reveal(normalization);
        normalization->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QTRY_COMPARE(converter.state(), "idle");
        QCOMPARE(output->property("text").toString(), "");
        auto* letterCase = item("conversionCase");
        reveal(letterCase);
        letterCase->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        auto* marks = item("conversionMarks");
        reveal(marks);
        marks->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(converter.state(), "idle");
        reveal(convert);
        click("convertText");
        QTRY_COMPARE(output->property("text").toString(), "tieng viet — dang 😀\nabc");
        QCOMPARE(input->property("text").toString(), source);
        click("nav-about");
        click("nav-convert");
        QCOMPARE(input->property("text").toString(), source); // Navigation keeps the draft only in memory.
        QCOMPARE(output->property("text").toString(), "tieng viet — dang 😀\nabc");
        input->setProperty("text", QStringLiteral("Nguồn mới"));
        QTRY_COMPARE(output->property("text").toString(), "");
        QCOMPARE(converter.state(), "idle");
        reveal(clear);
        clear->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(input->property("text").toString(), "");
        QCOMPARE(output->property("text").toString(), "");
        QVERIFY(!convert->isEnabled());
        input->setProperty("text", QString(TextConverter::inputLimit + 1, QLatin1Char('a')));
        QTRY_VERIFY(!convert->isEnabled());
        QVERIFY(item("conversionFeedback")->property("text").toString().contains("65536"));
        QCOMPARE(converter.state(), "idle");
        input->setProperty("text", QString());
        window->resize(720, 560);
        reveal(output);
        QVERIFY(output->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
        input->setProperty("text", QStringLiteral("Thử chưa có backend"));
        QTRY_VERIFY(!convert->isEnabled());
        QVERIFY(item("conversionFeedback")->property("text").toString().contains("chưa sẵn sàng"));
    }
    void cancelConversion_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void cancelConversion() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        TextConverter converter;
        UiClipboard clipboard;
        ClipboardBridge bridge(&clipboard);
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        window->setProperty("clipboardBridge", QVariant::fromValue(&bridge));
        click("nav-convert");
        auto* input = item("conversionInput");
        auto* run = item("convertText");
        auto* cancel = item("cancelConversion");
        auto* copy = item("copyConversionResult");
        QVERIFY(input && run && cancel && copy);
        input->setProperty("text", QStringLiteral("Nguồn giữ nguyên"));
        {
            ConversionGate gate;
            QVERIFY(gate.entered());
            reveal(run);
            click("convertText");
            QCOMPARE(converter.state(), "running");
            QVERIFY(!run->isEnabled());
            QVERIFY(cancel->isEnabled());
            QVERIFY(!copy->isEnabled());
            click("nav-about");
            QCOMPARE(window->property("pageIndex").toInt(), 7);
            QCOMPARE(converter.state(), "running"); // Navigation does not cancel or block.
            click("nav-convert");
            reveal(cancel);
            cancel->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_Space);
            QCOMPARE(converter.state(), "cancelled");
            QVERIFY(converter.busy());
            QVERIFY(!run->isEnabled());
            QVERIFY(!cancel->isEnabled());
            QCOMPARE(input->property("text").toString(), "Nguồn giữ nguyên");
            gate.release();
            QTRY_VERIFY(!converter.busy());
            QCOMPARE(converter.state(), "cancelled");
            QVERIFY(converter.result().isEmpty());
            QTRY_VERIFY(run->isEnabled());
        }
        {
            ConversionGate gate;
            QVERIFY(gate.entered());
            reveal(run);
            click("convertText");
            QCOMPARE(converter.state(), "running");
            input->setProperty("text", QStringLiteral("Bản nháp mới"));
            QCOMPARE(converter.state(), "idle");
            gate.release();
            QTRY_VERIFY(!converter.busy());
            QCOMPARE(converter.state(), "idle");
            QVERIFY(converter.result().isEmpty());
            QCOMPARE(input->property("text").toString(), "Bản nháp mới");
        }
        {
            ConversionGate gate;
            QVERIFY(gate.entered());
            reveal(run);
            click("convertText");
            QCOMPARE(converter.state(), "running");
            auto* normalization = item("conversionNormalization");
            reveal(normalization);
            normalization->forceActiveFocus();
            QTest::keyClick(window, Qt::Key_End);
            QCOMPARE(converter.state(), "idle");
            QVERIFY(converter.busy());
            reveal(item("clearConversion"));
            click("clearConversion");
            QCOMPARE(input->property("text").toString(), "");
            gate.release();
            QTRY_VERIFY(!converter.busy());
            QCOMPARE(converter.state(), "idle");
            QVERIFY(converter.result().isEmpty());
        }
        input->setProperty("text", QStringLiteral("Bản nháp mới"));
        reveal(run);
        click("convertText");
        QTRY_COMPARE(converter.state(), "converted");
        QCOMPARE(converter.result(), QStringLiteral("Bản nháp mới").normalized(QString::NormalizationForm_D));
        QCOMPARE(clipboard.reads + clipboard.writes, 0);
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
    }
    void aboutPage_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void aboutPage() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        click("nav-about");
        auto* toggle = item("toggleAboutLicense");
        auto* license = item("aboutLicenseText");
        auto* version = item("aboutVersionCard");
        QVERIFY(toggle && license && version);
        QVERIFY(!toggle->isEnabled());
        QVERIFY(version->property("description").toString().contains("Chưa xác định"));
        AboutInfo info;
        window->setProperty("aboutInfo", QVariant::fromValue(&info));
        QTRY_VERIFY(toggle->isEnabled());
        QVERIFY(version->property("description").toString().contains(info.sourceVersion()));
        QVERIFY(version->property("description").toString().contains("không phải phiên bản addon"));
        QCOMPARE(info.licenseState(), "idle"); // Page navigation never loads or downloads anything.
        QVERIFY(license->property("text").toString().isEmpty());
        reveal(toggle);
        toggle->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(info.licenseState(), "ready");
        QCOMPARE(license->property("text").toString(), info.licenseText());
        QVERIFY(license->property("readOnly").toBool());
        QCOMPARE(license->property("textFormat").toInt(), 0); // TextEdit.PlainText
        QVERIFY(item("aboutProjectSources")->property("readOnly").toBool());
        QCOMPARE(item("aboutProjectSources")->property("textFormat").toInt(), 0);
        window->resize(720, 560);
        reveal(license);
        QVERIFY(license->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        reveal(toggle);
        click("toggleAboutLicense");
        QTRY_COMPARE(info.licenseState(), "idle");
        QVERIFY(license->property("text").toString().isEmpty());
        window->setProperty("aboutInfo", QVariant::fromValue<QObject*>(nullptr));
        QTRY_VERIFY(!toggle->isEnabled());
    }
    void conversionClipboard() {
        UiClipboard fake;
        ClipboardBridge bridge(&fake);
        TextConverter converter;
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        window->setProperty("clipboardBridge", QVariant::fromValue(&bridge));
        click("nav-convert");
        auto* paste = item("pasteConversionSource");
        auto* copy = item("copyConversionResult");
        auto* input = item("conversionInput");
        auto* convert = item("convertText");
        QVERIFY(paste && copy && input && convert);
        QCOMPARE(fake.reads + fake.writes, 0);
        QVERIFY(!copy->isEnabled());
        fake.data.setText(QStringLiteral("Tiếng Việt 😀"));
        reveal(paste);
        click("pasteConversionSource");
        QTRY_COMPARE(input->property("text").toString(), QStringLiteral("Tiếng Việt 😀"));
        QCOMPARE(fake.reads, 1);
        QCOMPARE(fake.writes, 0);
        QCOMPARE(converter.state(), "idle");
        QVERIFY(!copy->isEnabled());
        reveal(convert);
        click("convertText");
        QTRY_VERIFY(copy->isEnabled());
        reveal(copy);
        copy->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QCOMPARE(fake.writes, 1);
        QCOMPARE(fake.written, converter.result());
        QCOMPARE(fake.reads, 1);
        input->setProperty("text", QStringLiteral("Draft mới"));
        QTRY_VERIFY(!copy->isEnabled());
        QCOMPARE(fake.writes, 1);
        const auto savedClipboard = fake.written;
        fake.data.setText(QString(TextConverter::inputLimit + 1, QLatin1Char('a')));
        reveal(paste);
        click("pasteConversionSource");
        QCOMPARE(input->property("text").toString(), "Draft mới");
        QVERIFY(item("conversionClipboardFeedback")->property("text").toString().contains("vượt giới hạn"));
        QCOMPARE(fake.written, savedClipboard);
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
        QTRY_VERIFY(!paste->isEnabled());
        QVERIFY(!copy->isEnabled());
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
    }
    void fileConversionActions() {
        FileConversionDouble converter;
        window->setProperty("fileConversion", QVariant::fromValue(&converter));
        window->setProperty("pageIndex", 6);
        auto* card = item("fileConversionSettings");
        card->setProperty("inputFile", QUrl::fromLocalFile("/tmp/source.txt"));
        card->setProperty("outputFile", QUrl::fromLocalFile("/tmp/result.txt"));
        auto* target = item("fileConversionTarget"); reveal(target); target->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(converter.starts, 0);
        reveal(item("startFileConversion")); click("startFileConversion");
        QCOMPARE(converter.starts, 1); QCOMPARE(converter.source, "unicode"); QCOMPARE(converter.target, "cp1258");
        QVERIFY(!item("startFileConversion")->isEnabled());
        QVERIFY(!item("chooseConversionInput")->isEnabled());
        click("cancelFileConversion"); QCOMPARE(converter.cancels, 1);
        window->setProperty("fileConversion", QVariant::fromValue<QObject*>(nullptr));
    }
    void advancedControls() {
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        for (const auto& key : {QString("FreeMark"), QString("AllowConsonantZFWJ"), QString("QuickStartConsonant"), QString("QuickEndConsonant"), QString("RestoreWrongSpelling"), QString("RememberInputMethodPerApplication")}) {
            window->setProperty("pageIndex", key == "RememberInputMethodPerApplication" ? 3 : key == "FreeMark" || key == "AllowConsonantZFWJ" ? 4 : 1);
            auto prefix = key; prefix[0] = prefix[0].toLower();
            auto* picker = item(qPrintable(prefix + "Picker")); QVERIFY(picker);
            reveal(picker); picker->forceActiveFocus(); QTest::keyClick(window, Qt::Key_End);
            QCOMPARE(controller.advanced.value(key).toString(), "False");
            reveal(item(qPrintable("apply" + key))); click(qPrintable("apply" + key));
            QCOMPARE(controller.advanced.value(key).toString(), "True");
            QCOMPARE(controller.applies, 0);
        }
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void inputMethodSelector_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light"); QTest::newRow("dark") << QString("dark");
    }
    void inputMethodSelector() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        InputMethodsDouble methods;
        window->setProperty("inputMethodController", QVariant::fromValue(&methods));
        auto* picker = item("inputMethodPicker");
        QTRY_COMPARE(picker->property("count").toInt(), 5);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 1);
        reveal(picker); picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(methods.selections, 0);
        QCOMPARE(picker->property("currentIndex").toInt(), 4);
        reveal(item("selectInputMethod")); click("selectInputMethod");
        QCOMPARE(methods.selections, 1);
        QCOMPARE(methods.current, "openkey-simple-telex2");
        QVERIFY(item("currentInputMethod")->property("text").toString().contains("Simple Telex 2"));
        methods.current = "openkey-vni"; emit methods.changed();
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 2);
        methods.busy = true; emit methods.changed();
        QTRY_VERIFY(!item("selectInputMethod")->isEnabled());
        QVERIFY(!item("reloadInputMethods")->isEnabled());
        methods.busy = false; emit methods.changed();
        reveal(item("reloadInputMethods")); click("reloadInputMethods");
        QCOMPARE(methods.reloads, 1); QCOMPARE(methods.selections, 1);
        reveal(item("exportInputMethod")); click("exportInputMethod");
        QVERIFY(item("inputMethodBackupJson")->property("text").toString().contains("openkey-input-method"));
        item("inputMethodBackupJson")->setProperty("text", "{\"format\":\"openkey-input-method\",\"version\":1,\"method\":\"openkey-simple-telex1\"}");
        reveal(item("inspectInputMethodBackup")); click("inspectInputMethodBackup");
        QCOMPARE(methods.selections, 1);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 3);
        methods.methods = {"keyboard-us", "openkey-vni"}; emit methods.changed();
        QTRY_COMPARE(picker->property("count").toInt(), 2);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 1);
        window->setProperty("inputMethodController", QVariant::fromValue<QObject*>(nullptr));
        QTRY_VERIFY(!picker->isEnabled());
    }
    void applicationRulesActions() {
        StyleControllerDouble controller;
        MacroModel rules(nullptr, true);
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("applicationRulesModel", QVariant::fromValue(&rules));
        window->setProperty("pageIndex", 3);
        reveal(item("loadApplicationRules")); click("loadApplicationRules");
        item("applicationProgram")->setProperty("text", "org.test.Editor");
        reveal(item("putApplicationRule")); click("putApplicationRule");
        QCOMPARE(rules.entries().size(), 1);
        QVERIFY(controller.appRules.isEmpty());
        reveal(item("applyApplicationRules")); click("applyApplicationRules");
        QCOMPARE(controller.appRules, rules.encoded());
        item("applicationProgram")->setProperty("text", "gnome-shell");
        reveal(item("putApplicationRule")); click("putApplicationRule");
        QCOMPARE(rules.entries().size(), 1);
        controller.appRules = "636f6465:50726565646974;";
        emit controller.changed();
        QTRY_VERIFY(!item("applyApplicationRules")->isEnabled());
        window->setProperty("applicationRulesModel", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void hotkeyEditorActions() {
        HotkeyOverviewDouble hotkeys;
        hotkeys.shortcuts = {{"TriggerKeys", QStringList{"Control+space"}}};
        window->setProperty("hotkeyOverview", QVariant::fromValue(&hotkeys));
        window->setProperty("pageIndex", 2);
        reveal(item("loadHotkeyDraft")); click("loadHotkeyDraft");
        QCOMPARE(item("hotkeyEditKeys")->property("text").toString(), "Control+space");
        item("hotkeyEditKeys")->setProperty("text", "Control+Alt+v");
        QCOMPARE(hotkeys.writes, 0);
        reveal(item("applyHotkeyDraft")); click("applyHotkeyDraft");
        QCOMPARE(hotkeys.writes, 1);
        QCOMPARE(hotkeys.shortcuts.value("TriggerKeys").toStringList(), QStringList{"Control+Alt+v"});
        hotkeys.state = "applying"; emit hotkeys.changed();
        QTRY_VERIFY(!item("applyHotkeyDraft")->isEnabled());
        QVERIFY(!item("reloadHotkeys")->isEnabled());
        window->setProperty("hotkeyOverview", QVariant::fromValue<QObject*>(nullptr));
    }
    void macroEditorActions() {
        StyleControllerDouble controller;
        MacroModel macros;
        BackupFiles files;
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("macroModel", QVariant::fromValue(&macros));
        window->setProperty("backupFiles", QVariant::fromValue(&files));
        window->setProperty("pageIndex", 5);
        reveal(item("loadMacros"));
        click("loadMacros");
        item("macroKey")->setProperty("text", "vn");
        item("macroValue")->setProperty("text", "Việt Nam");
        reveal(item("putMacro"));
        click("putMacro");
        QCOMPARE(macros.entries().size(), 1);
        QVERIFY(controller.macroTable.isEmpty());
        reveal(item("exportMacros"));
        click("exportMacros");
        const auto json = item("macroJson")->property("text").toString();
        QVERIFY(json.contains("openkey-macros"));
        item("macroJson")->setProperty("text", "{}");
        reveal(item("importMacros"));
        click("importMacros");
        QCOMPARE(macros.entries().size(), 1);
        reveal(item("applyMacros"));
        click("applyMacros");
        QCOMPARE(controller.macroTable, macros.encoded());
        window->setProperty("pageIndex", 0);
        window->setProperty("pageIndex", 5);
        QCOMPARE(macros.entries().size(), 1);
        QCOMPARE(item("macroJson")->property("text").toString(), "{}");
        QTemporaryDir dir;
        const auto url = QUrl::fromLocalFile(dir.filePath("gõ tắt.json"));
        item("macroJson")->setProperty("text", json);
        QVERIFY(QMetaObject::invokeMethod(item("macroSettings"), "saveFile", Q_ARG(QVariant, url)));
        QCOMPARE(files.loadMacros(url).value("text").toString(), json);
        controller.macroTable = "61:62;";
        emit controller.changed();
        macros.put("x", "xin chào");
        QTRY_VERIFY(!item("applyMacros")->isEnabled());
        window->setProperty("macroModel", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("backupFiles", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void nativeDirectStyleControls() {
        StyleControllerDouble controller;
        controller.supportsCommitOnly = true;
        controller.supportsNativeDirect = true;
        window->setProperty("configController", QVariant::fromValue(&controller));
        auto* picker = item("inputStylePicker");
        QTRY_COMPARE(picker->property("count").toInt(), 4);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.applies, 0);
        QCOMPARE(picker->property("currentIndex").toInt(), 3);
        click("applyInputStyle");
        QCOMPARE(controller.style, "NativeDirect");
        QCOMPARE(controller.applies, 1);
        controller.style = "CommitOnly";
        controller.supportsNativeDirect = false;
        emit controller.changed();
        QTRY_COMPARE(picker->property("count").toInt(), 3);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 2);
    }

    void commitOnlyStyleControls() {
        StyleControllerDouble controller;
        controller.supportsCommitOnly = true;
        window->setProperty("configController", QVariant::fromValue(&controller));
        auto* picker = item("inputStylePicker");
        QTRY_COMPARE(picker->property("count").toInt(), 3);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.applies, 0);
        QCOMPARE(picker->property("currentIndex").toInt(), 2);
        click("applyInputStyle");
        QCOMPARE(controller.style, "CommitOnly");
        QCOMPARE(controller.applies, 1);
        QCOMPARE(picker->property("currentIndex").toInt(), 2);
        controller.style = "Preedit";
        controller.supportsCommitOnly = false;
        emit controller.changed();
        QTRY_COMPARE(picker->property("count").toInt(), 2);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 0);
    }

    void uiBackupFileActions_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void uiBackupFileActions() {
        QFETCH(QString, theme);
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const auto config = dir.filePath("ui.ini");
        UiPreferences preferences(config);
        preferences.load();
        BackupFiles files;
        window->setProperty("themeMode", theme);
        window->setProperty("uiPreferences", QVariant::fromValue(&preferences));
        window->setProperty("backupFiles", QVariant::fromValue(&files));
        click("nav-advanced");
        QTest::qWait(50);
        auto* card = item("preferencesBackup");
        auto* text = item("uiBackupText");
        auto* open = item("openUiBackupFile");
        auto* save = item("saveUiBackupFile");
        auto* restore = item("restoreUiBackup");
        QVERIFY(card && text && open && save && restore);
        QVERIFY(open->isEnabled());
        QVERIFY(!save->isEnabled());
        reveal(item("exportUiBackup"));
        click("exportUiBackup");
        const auto json = text->property("text").toString();
        QVERIFY(files.validUiBackup(json));
        QVERIFY(save->isEnabled());
        auto* openDialog = window->findChild<QObject*>("uiBackupOpenDialog");
        auto* saveDialog = window->findChild<QObject*>("uiBackupSaveDialog");
        QVERIFY(openDialog && saveDialog);
        reveal(open);
        click("openUiBackupFile");
        QTRY_VERIFY(openDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(openDialog, "reject"));
        QTRY_VERIFY(!openDialog->property("visible").toBool());
        QCOMPARE(text->property("text").toString(), json);
        reveal(save);
        click("saveUiBackupFile");
        QTRY_VERIFY(saveDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(saveDialog, "reject"));
        QTRY_VERIFY(!saveDialog->property("visible").toBool());
        QVERIFY(!QFile::exists(config));
        const auto url = QUrl::fromLocalFile(dir.filePath("ui-backup.json"));
        QVERIFY(QMetaObject::invokeMethod(card, "saveFile", Q_ARG(QVariant, QVariant(url))));
        QCOMPARE(files.loadUi(url).value("text").toString(), json);
        const auto targetTheme = theme == "light" ? QString("dark") : QString("light");
        const auto imported = preferences.backupJson(targetTheme, false);
        QCOMPARE(files.saveUi(url, imported), "saved");
        QVERIFY(QMetaObject::invokeMethod(card, "loadFile", Q_ARG(QVariant, QVariant(url))));
        QCOMPARE(text->property("text").toString(), imported);
        QCOMPARE(window->property("themeMode").toString(), theme);
        QVERIFY(!restore->isEnabled());
        QVERIFY(!QFile::exists(config));
        // A valid input-method backup is still the wrong format for this pane.
        const auto inputJson = InputConfigBackup::encode({{"InputStyle", "Direct"}, {"DirectDiagnostics", "False"}});
        QCOMPARE(files.save(url, inputJson), "saved");
        QVERIFY(QMetaObject::invokeMethod(card, "loadFile", Q_ARG(QVariant, QVariant(url))));
        QCOMPARE(text->property("text").toString(), imported);
        QVERIFY(item("uiBackupFeedback")->property("text").toString().contains("JSON bộ gõ"));
        QVERIFY(!restore->isEnabled());
        reveal(item("previewUiBackup"));
        click("previewUiBackup");
        QVERIFY(restore->isEnabled());
        reveal(restore);
        click("restoreUiBackup");
        QCOMPARE(window->property("themeMode").toString(), targetTheme);
        QCOMPARE(window->property("sidebarExpanded").toBool(), false);
        QVERIFY(QFile::exists(config));
        QCOMPARE(preferences.status(), QString("saved"));
        text->setProperty("text", QString("{}"));
        QVERIFY(!save->isEnabled());
        window->setProperty("backupFiles", QVariant::fromValue<QObject*>(nullptr));
        QVERIFY(!open->isEnabled());
        window->setProperty("uiPreferences", QVariant::fromValue<QObject*>(nullptr));
    }
    void inputBackupFileActions() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        BackupFiles files;
        window->setProperty("backupFiles", QVariant::fromValue(&files));
        click("nav-advanced");
        QTest::qWait(50);
        auto* card = item("inputSettingsBackup");
        auto* text = item("inputBackupText");
        auto* save = item("saveInputBackupFile");
        auto* open = item("openInputBackupFile");
        auto* restore = item("restoreInputBackup");
        QVERIFY(card && text && save && open && restore);
        QVERIFY(open->isEnabled());
        QVERIFY(!save->isEnabled());
        const auto json = InputConfigBackup::encode({{"InputStyle", "Direct"}, {"DirectDiagnostics", "False"}});
        text->setProperty("text", json);
        QVERIFY(save->isEnabled());
        auto* openDialog = window->findChild<QObject*>("inputBackupOpenDialog");
        auto* saveDialog = window->findChild<QObject*>("inputBackupSaveDialog");
        QVERIFY(openDialog && saveDialog);
        reveal(open);
        click("openInputBackupFile");
        QTRY_VERIFY(openDialog->property("visible").toBool());
        QVERIFY(QMetaObject::invokeMethod(openDialog, "reject"));
        QTRY_VERIFY(!openDialog->property("visible").toBool());
        QCOMPARE(text->property("text").toString(), json);
        reveal(save);
        click("saveInputBackupFile");
        QTRY_VERIFY(saveDialog->property("visible").toBool());
        QCOMPARE(saveDialog->property("defaultSuffix").toString(), QString("json"));
        QVERIFY(QMetaObject::invokeMethod(saveDialog, "reject"));
        QTRY_VERIFY(!saveDialog->property("visible").toBool());
        QCOMPARE(QDir(dir.path()).entryList(QDir::Files | QDir::NoDotAndDotDot).size(), 0);
        const auto path = dir.filePath("backup.json");
        const auto url = QUrl::fromLocalFile(path);
        // Exercise accepted-path handlers without driving a native chooser.
        QVERIFY(QMetaObject::invokeMethod(card, "saveFile", Q_ARG(QVariant, QVariant(url))));
        QVERIFY(QFile::exists(path));
        text->setProperty("text", QString("unfinished draft"));
        QVERIFY(!save->isEnabled());
        QVERIFY(QMetaObject::invokeMethod(card, "loadFile", Q_ARG(QVariant, QVariant(url))));
        QCOMPARE(text->property("text").toString(), json);
        QVERIFY(!restore->isEnabled()); // Import never authorizes a restore.
        const auto missing = QUrl::fromLocalFile(dir.filePath("missing.json"));
        QVERIFY(QMetaObject::invokeMethod(card, "loadFile", Q_ARG(QVariant, QVariant(missing))));
        QCOMPARE(text->property("text").toString(), json);
        QVERIFY(item("inputBackupFeedback")->property("text").toString().contains("Giữ nguyên"));
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        controller.busy = true;
        emit controller.changed();
        QVERIFY(!save->isEnabled());
        QVERIFY(!open->isEnabled());
        text->setProperty("text", QString("keep"));
        QVERIFY(QMetaObject::invokeMethod(card, "loadFile", Q_ARG(QVariant, QVariant(url))));
        QCOMPARE(text->property("text").toString(), QString("keep"));
        QCOMPARE(controller.backupRestores, 0);
        QCOMPARE(controller.applies, 0);
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("backupFiles", QVariant::fromValue<QObject*>(nullptr));
        QVERIFY(!open->isEnabled());
    }
    void inputSettingsBackup_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void inputSettingsBackup() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        click("nav-advanced");
        auto* text = item("inputBackupText");
        auto* restore = item("restoreInputBackup");
        auto* inspect = item("previewInputBackup");
        QVERIFY(text && restore && inspect);
        QVERIFY(!item("exportInputBackup")->isEnabled());
        QVERIFY(!restore->isEnabled());
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        reveal(item("exportInputBackup"));
        click("exportInputBackup");
        const auto json = text->property("text").toString();
        QVERIFY(json.contains("openkey-input-settings"));
        QCOMPARE(controller.backupRestores, 0);
        controller.style = "Direct";
        emit controller.changed();
        reveal(inspect);
        click("previewInputBackup");
        QVERIFY(restore->isEnabled());
        QCOMPARE(controller.backupInspections, 1);
        QVERIFY(item("inputBackupPreview")->property("text").toString().contains("Direct → Preedit"));
        text->setProperty("text", json + " ");
        QVERIFY(!restore->isEnabled());
        reveal(inspect);
        click("previewInputBackup");
        QVERIFY(restore->isEnabled());
        emit controller.changed();
        QVERIFY(!restore->isEnabled());
        reveal(inspect);
        click("previewInputBackup");
        reveal(restore);
        click("restoreInputBackup");
        QCOMPARE(controller.backupRestores, 1);
        QCOMPARE(controller.style, "Preedit");
        QVERIFY(!restore->isEnabled());
        QCOMPARE(controller.applies, 0);
        QCOMPARE(controller.diagnosticsApplies, 0);
        click("nav-overview");
        click("nav-advanced");
        QCOMPARE(text->property("text").toString(), json + " ");
        text->setProperty("text", QString("{}"));
        reveal(inspect);
        click("previewInputBackup");
        QVERIFY(!restore->isEnabled());
        QVERIFY(item("inputBackupFeedback")->property("text").toString().contains("không hợp lệ"));
        text->setProperty("text", QString(4097, 'x'));
        QVERIFY(!inspect->isEnabled());
        window->resize(720, 560);
        reveal(restore);
        QVERIFY(item("inputSettingsBackup")->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void runtimeStyleSynchronization() {
        StyleControllerDouble controller;
        controller.supportsCommitOnly = true;
        controller.supportsNativeDirect = true;
        window->setProperty("configController", QVariant::fromValue(&controller));
        auto* picker = item("inputStylePicker");
        auto* label = item("inputStyleRuntimeLabel");
        QVERIFY(picker && label);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 0);
        // Simulate readback after another configuration window changed Fcitx.
        controller.style = "NativeDirect";
        QVERIFY(QMetaObject::invokeMethod(window, "synchronizeRuntime"));
        QCOMPARE(controller.reloads, 1);
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 3);
        QTRY_VERIFY(label->property("text").toString().contains("NativeDirect"));
        QCOMPARE(controller.applies, 0);
        controller.busy = true;
        QVERIFY(QMetaObject::invokeMethod(window, "synchronizeRuntime"));
        QCOMPARE(controller.reloads, 1); // Never overlap a write/read in flight.
        controller.busy = false;
        controller.style = "CommitOnly";
        QVERIFY(QMetaObject::invokeMethod(window, "synchronizeRuntime"));
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 2);
        QVERIFY(!item("applyInputStyle")->isEnabled());
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        QVERIFY(QMetaObject::invokeMethod(window, "synchronizeRuntime"));
        QCOMPARE(controller.reloads, 2);
        QCOMPARE(controller.applies, 0);
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
        reveal(apply);
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
        QTest::addColumn<bool>("compatibilityPage");
        for (const auto& theme : {"light", "dark"}) {
            for (const auto& pickerName : {"inputStylePicker", "inputMethodPicker", "themePicker", "diagnosticsPicker",
                                           "checkSpellingPicker", "modernOrthographyPicker", "quickTelexPicker",
                                           "conversionNormalization", "conversionCase", "conversionMarks"}) {
                const QByteArray name = QByteArray(theme) + '-' + pickerName;
                QTest::newRow(name.constData()) << QString(theme) << QString(pickerName) << false;
            }
            QTest::newRow(qPrintable(QString(theme) + "-compatibility-style"))
                << QString(theme) << QString("inputStylePicker") << true;
        }
    }
    void popupContrast() {
        QFETCH(QString, theme);
        QFETCH(QString, pickerName);
        QFETCH(bool, compatibilityPage);
        InputMethodsDouble methods;
        window->setProperty("inputMethodController", QVariant::fromValue(&methods));
        StyleControllerDouble controller;
        controller.style = "Direct"; // Reproduce the selected row in the report.
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("themeMode", theme);
        if (compatibilityPage) {
            click("nav-compatibility");
            // The test area becomes visible on this page; let the shared
            // column finish its new layout before scrolling to the picker.
            QTest::qWait(50);
            reveal(item("inputStylePicker"));
        }
        if (pickerName.startsWith("conversion")) {
            click("nav-convert");
            reveal(item(qPrintable(pickerName)));
        }
        if (pickerName == "diagnosticsPicker") {
            click("nav-advanced");
            QTest::qWait(50);
        }
        if (pickerName == "checkSpellingPicker" || pickerName == "modernOrthographyPicker" || pickerName == "quickTelexPicker") {
            click("nav-spelling");
            QTest::qWait(50);
            reveal(item(qPrintable(pickerName)));
        }
        auto* picker = item(qPrintable(pickerName));
        QVERIFY(picker);
        if (pickerName != "themePicker") reveal(picker);
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
        QCOMPARE(controller.diagnosticsApplies, 0);
        QCOMPARE(controller.spellingApplies, 0);
        QCOMPARE(controller.modernApplies, 0);
        QCOMPARE(controller.quickTelexApplies, 0);
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void diagnosticsControls() {
        click("nav-advanced");
        QTest::qWait(50);
        auto* picker = item("diagnosticsPicker");
        auto* apply = item("applyDiagnostics");
        auto* reload = item("reloadDiagnostics");
        QVERIFY(picker && apply && reload);
        QVERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!reload->isEnabled());
        StyleControllerDouble controller;
        window->setProperty("configController", QVariant::fromValue(&controller));
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 0);
        QVERIFY(!apply->isEnabled());
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.diagnosticsApplies, 0);
        QTRY_VERIFY(apply->isEnabled());
        click("applyDiagnostics");
        QCOMPARE(controller.diagnosticsApplies, 1);
        QCOMPARE(controller.diagnosticsValue, "True");
        QCOMPARE(controller.style, "Preedit");
        QCOMPARE(controller.applies, 0);
        QTRY_VERIFY(!apply->isEnabled());
        QVERIFY(item("runtimeDiagnosticsLabel")->property("text").toString().contains("Bật"));
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        click("applyDiagnostics");
        QCOMPARE(controller.diagnosticsValue, "False");
        QCOMPARE(controller.diagnosticsApplies, 2);
        click("reloadDiagnostics");
        QCOMPARE(controller.reloads, 1);
        controller.busy = true;
        emit controller.changed();
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(!reload->isEnabled());
        QVERIFY(!apply->isEnabled());
        controller.busy = false;
        controller.ready = false;
        controller.result = "unavailable";
        controller.diagnosticsValue.clear();
        emit controller.changed();
        QVERIFY(item("diagnosticsFeedback")->property("text").toString().contains("Không kết nối"));
        QTRY_COMPARE(picker->property("currentIndex").toInt(), -1);
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void uiBackup_data() {
        QTest::addColumn<QString>("theme");
        QTest::newRow("light") << QString("light");
        QTest::newRow("dark") << QString("dark");
    }
    void uiBackup() {
        QFETCH(QString, theme);
        window->setProperty("themeMode", theme);
        const auto path = directory.filePath("ui-backup-" + theme + "/ui.ini");
        UiPreferences preferences(path);
        preferences.load();
        StyleControllerDouble controller;
        UiClipboard clipboard;
        ClipboardBridge bridge(&clipboard);
        TextConverter converter;
        window->setProperty("configController", QVariant::fromValue(&controller));
        window->setProperty("clipboardBridge", QVariant::fromValue(&bridge));
        window->setProperty("textConverter", QVariant::fromValue(&converter));
        click("nav-convert");
        item("conversionInput")->setProperty("text", QStringLiteral("Bản nháp không nằm trong sao lưu"));
        click("nav-advanced");
        auto* text = item("uiBackupText");
        auto* exportButton = item("exportUiBackup");
        auto* preview = item("previewUiBackup");
        auto* restore = item("restoreUiBackup");
        QVERIFY(text && exportButton && preview && restore);
        QVERIFY(!exportButton->isEnabled()); // Offline preview never persists settings.
        QVERIFY(!preview->isEnabled());
        QVERIFY(!restore->isEnabled());
        window->setProperty("uiPreferences", QVariant::fromValue(&preferences));
        QVERIFY(text->property("text").toString().isEmpty());
        reveal(exportButton);
        click("exportUiBackup");
        const auto json = QJsonDocument::fromJson(text->property("text").toString().toUtf8()).object();
        QCOMPARE(json.size(), 4);
        QCOMPARE(json["theme"].toString(), theme);
        QVERIFY(json["sidebarExpanded"].toBool());
        QVERIFY(!QFile::exists(path));
        QVERIFY(!restore->isEnabled());
        const auto targetTheme = theme == "light" ? QStringLiteral("dark") : QStringLiteral("light");
        const auto backup = preferences.backupJson(targetTheme, false);
        text->setProperty("text", backup);
        reveal(preview);
        preview->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_VERIFY(restore->isEnabled());
        QVERIFY(item("uiBackupPreview")->property("text").toString().contains(targetTheme));
        QCOMPARE(window->property("themeMode").toString(), theme);
        QVERIFY(window->property("sidebarExpanded").toBool());
        QVERIFY(!QFile::exists(path));
        text->setProperty("text", "{broken json");
        QVERIFY(!restore->isEnabled());
        QVERIFY(!item("uiBackupPreview")->isVisible());
        reveal(preview);
        click("previewUiBackup");
        QVERIFY(!restore->isEnabled());
        QVERIFY(item("uiBackupFeedback")->property("text").toString().contains("không hợp lệ"));
        text->setProperty("text", QString(4097, QLatin1Char(' ')));
        click("previewUiBackup");
        QVERIFY(item("uiBackupFeedback")->property("text").toString().contains("4096"));
        QVERIFY(!QFile::exists(path));
        text->setProperty("text", backup);
        click("previewUiBackup");
        reveal(restore);
        restore->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Space);
        QTRY_COMPARE(window->property("themeMode").toString(), targetTheme);
        QVERIFY(!window->property("sidebarExpanded").toBool());
        QCOMPARE(preferences.status(), "saved");
        QVERIFY(!restore->isEnabled());
        UiPreferences reopened(path);
        reopened.load();
        QCOMPARE(reopened.themeMode(), targetTheme);
        QVERIFY(!reopened.sidebarExpanded());
        window->resize(720, 560);
        reveal(text);
        QVERIFY(text->width() <= item("contentScroll")->width());
        QVERIFY(!window->grabWindow().isNull());
        click("nav-convert");
        QCOMPARE(item("conversionInput")->property("text").toString(), "Bản nháp không nằm trong sao lưu");
        QCOMPARE(converter.state(), "idle");
        QCOMPARE(clipboard.reads + clipboard.writes, 0);
        QCOMPARE(controller.applies + controller.diagnosticsApplies + controller.spellingApplies +
            controller.modernApplies + controller.quickTelexApplies + controller.reloads, 0);
        window->setProperty("uiPreferences", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("clipboardBridge", QVariant::fromValue<QObject*>(nullptr));
        window->setProperty("textConverter", QVariant::fromValue<QObject*>(nullptr));
    }
    void uiBackupWriteFailure() {
        const auto parent = directory.filePath("backup-blocked-parent");
        QFile file(parent);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write("keep"), 4);
        file.close();
        UiPreferences preferences(parent + "/ui.ini");
        preferences.load();
        window->setProperty("uiPreferences", QVariant::fromValue(&preferences));
        window->setProperty("themeMode", QStringLiteral("light"));
        click("nav-advanced");
        item("uiBackupText")->setProperty("text", preferences.backupJson("dark", false));
        QTest::qWait(50); // Let the newly visible, wrapped card update scroll geometry.
        reveal(item("previewUiBackup"));
        click("previewUiBackup");
        QVERIFY(item("restoreUiBackup")->isEnabled());
        reveal(item("restoreUiBackup"));
        click("restoreUiBackup");
        QCOMPARE(window->property("themeMode").toString(), "light");
        QVERIFY(window->property("sidebarExpanded").toBool());
        QCOMPARE(preferences.themeMode(), "system");
        QVERIFY(preferences.sidebarExpanded());
        QVERIFY(item("uiBackupFeedback")->property("text").toString().contains("giữ nguyên"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), "keep");
        window->setProperty("uiPreferences", QVariant::fromValue<QObject*>(nullptr));
    }
    void uiPreferencesActions() {
        const auto path = directory.path() + "/ui-preferences/ui.ini";
        UiPreferences preferences(path);
        preferences.load();
        window->setProperty("uiPreferences", QVariant::fromValue(&preferences));
        QVERIFY(!QFile::exists(path)); // Injecting/loading never saves by itself.
        auto* picker = item("themePicker");
        QVERIFY(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QTRY_COMPARE(window->property("themeMode").toString(), QString("dark"));
        QTRY_COMPARE(preferences.status(), QString("saved"));
        click("sidebarToggle");
        QVERIFY(!preferences.sidebarExpanded());
        UiPreferences reopened(path);
        reopened.load();
        QCOMPARE(reopened.themeMode(), "dark");
        QVERIFY(!reopened.sidebarExpanded());
        QVERIFY(item("uiPreferencesFeedback")->property("text").toString().contains("lưu riêng"));
        // A fresh window starts from persisted values without writing them.
        QQmlApplicationEngine restoredEngine;
        restoredEngine.setInitialProperties({{"themeMode", reopened.themeMode()},
            {"sidebarExpanded", reopened.sidebarExpanded()}});
        restoredEngine.load(QUrl::fromLocalFile(QStringLiteral(OPENKEY_SETTINGS_QML)));
        QVERIFY(!restoredEngine.rootObjects().isEmpty());
        auto* restored = restoredEngine.rootObjects().first();
        QCOMPARE(restored->property("themeMode").toString(), "dark");
        QVERIFY(!restored->property("sidebarExpanded").toBool());
        window->setProperty("uiPreferences", QVariant::fromValue<QObject*>(nullptr));
    }
    void spellingControls_data() {
        QTest::addColumn<bool>("modern");
        QTest::newRow("spelling") << false;
        QTest::newRow("orthography") << true;
    }
    void spellingControls() {
        QFETCH(bool, modern);
        const QString prefix = modern ? "modernOrthography" : "checkSpelling";
        const QString key = modern ? "UseModernOrthography" : "CheckSpelling";
        click("nav-spelling");
        QTest::qWait(50);
        auto* picker = item(qPrintable(prefix + "Picker"));
        auto* apply = item(qPrintable("apply" + key));
        QVERIFY(picker && apply);
        QVERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        StyleControllerDouble controller;
        controller.supportsSpelling = false;
        window->setProperty("configController", QVariant::fromValue(&controller));
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(item(qPrintable(prefix + "Feedback"))->property("text").toString().contains("alpha.10"));
        QVERIFY(item("inputStylePicker")->isEnabled()); // Old addon keeps existing features.
        controller.supportsSpelling = true;
        emit controller.changed();
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 1);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        QCOMPARE(controller.spellingApplies + controller.modernApplies, 0);
        QTRY_VERIFY(apply->isEnabled());
        reveal(apply);
        click(qPrintable("apply" + key));
        QCOMPARE(modern ? controller.modernOrthography : controller.checkSpelling, "False");
        QCOMPARE(modern ? controller.checkSpelling : controller.modernOrthography, "True");
        QCOMPARE(controller.spellingApplies + controller.modernApplies, 1);
        QCOMPARE(controller.applies + controller.diagnosticsApplies, 0);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        reveal(apply);
        click(qPrintable("apply" + key));
        QCOMPARE(modern ? controller.modernOrthography : controller.checkSpelling, "True");
        QCOMPARE(controller.spellingApplies + controller.modernApplies, 2);
        controller.busy = true;
        emit controller.changed();
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void quickTelexControls() {
        click("nav-spelling");
        auto* picker = item("quickTelexPicker");
        auto* apply = item("applyQuickTelex");
        auto* reload = item("reloadQuickTelex");
        QVERIFY(picker && apply && reload);
        QVERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!reload->isEnabled());
        StyleControllerDouble controller;
        controller.supportsQuickTelex = false; // alpha.10: other two fields still usable.
        window->setProperty("configController", QVariant::fromValue(&controller));
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(item("checkSpellingPicker")->isEnabled());
        QVERIFY(item("modernOrthographyPicker")->isEnabled());
        QVERIFY(item("quickTelexFeedback")->property("text").toString().contains("alpha.11"));
        controller.supportsQuickTelex = true;
        emit controller.changed();
        QTRY_COMPARE(picker->property("currentIndex").toInt(), 0);
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QCOMPARE(controller.quickTelexApplies, 0);
        QVERIFY(apply->isEnabled());
        reveal(apply);
        click("applyQuickTelex");
        QCOMPARE(controller.quickTelex, "True");
        QCOMPARE(controller.quickTelexApplies, 1);
        QCOMPARE(controller.applies + controller.diagnosticsApplies + controller.spellingApplies + controller.modernApplies, 0);
        QCOMPARE(controller.checkSpelling, "True");
        QCOMPARE(controller.modernOrthography, "True");
        QCOMPARE(controller.style, "Preedit");
        controller.supportsSpelling = false; // Independent optional capability.
        emit controller.changed();
        QTRY_VERIFY(picker->isEnabled());
        reveal(picker);
        picker->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_Home);
        reveal(apply);
        click("applyQuickTelex");
        QCOMPARE(controller.quickTelex, "False");
        QCOMPARE(controller.quickTelexApplies, 2);
        reveal(reload);
        click("reloadQuickTelex");
        QCOMPARE(controller.reloads, 1);
        controller.busy = true;
        emit controller.changed();
        QTRY_VERIFY(!picker->isEnabled());
        QVERIFY(!apply->isEnabled());
        QVERIFY(!reload->isEnabled());
        window->setProperty("configController", QVariant::fromValue<QObject*>(nullptr));
    }
    void uiPreferencesFailure() {
        const auto parent = directory.path() + "/blocked-ui-parent";
        QFile file(parent);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        UiPreferences preferences(parent + "/ui.ini");
        preferences.load();
        window->setProperty("uiPreferences", QVariant::fromValue(&preferences));
        item("themePicker")->forceActiveFocus();
        QTest::keyClick(window, Qt::Key_End);
        QTRY_COMPARE(window->property("themeMode").toString(), QString("dark"));
        QCOMPARE(preferences.status(), "error");
        QVERIFY(item("uiPreferencesFeedback")->property("text").toString().contains("Không lưu được"));
        window->setProperty("uiPreferences", QVariant::fromValue<QObject*>(nullptr));
    }
};

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Basic");
    LinuxSettingsTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "LinuxSettingsTests.moc"
