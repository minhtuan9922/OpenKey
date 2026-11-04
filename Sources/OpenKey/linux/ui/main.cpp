// Config writes only on explicit Apply through the fixed Fcitx D-Bus API.
#include "SettingsOverview.h"
#include "SettingsController.h"
#include "SettingsActivation.h"
#include "UiPreferences.h"
#include "HotkeyOverview.h"
#include "UiEnvironment.h"
#include "TextConverter.h"
#include "AboutInfo.h"
#include "ClipboardBridge.h"
#include <QGuiApplication>
#include <QCommandLineParser>
#include <QFileInfo>
#include <QQmlApplicationEngine>
#include <QQmlError>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <QImage>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>
#include <memory>

int main(int argc, char** argv) {
    QGuiApplication app(argc, argv);
    app.setApplicationName("OpenKey Settings Preview");
    app.setDesktopFileName("org.openkey.Settings");
    QCommandLineParser parser;
    parser.setApplicationDescription("Qt Quick settings preview; config writes only on explicit Apply.");
    parser.addHelpOption();
    parser.addOption({"smoke-test", "Load the embedded UI and exit; no desktop/engine changes."});
    parser.addOption({"screenshot", "Save a preview PNG to a new file, then exit.", "path"});
    parser.addOption({"theme", "Preview theme: system, light or dark (not saved).", "mode", "system"});
    parser.addOption({"page", "Initial page key.", "key", "overview"});
    parser.addOption({"offline", "Preview without reading config or connecting to the session service."});
    parser.process(app);
    const QStringList themes{"system", "light", "dark"};
    const QStringList pages{"overview", "spelling", "hotkeys", "compatibility", "advanced", "macro", "convert", "about"};
    if (!themes.contains(parser.value("theme")) || !pages.contains(parser.value("page")) ||
        !parser.positionalArguments().isEmpty()) { qCritical() << "Invalid theme/page/arguments"; return 2; }
    const auto screenshot = parser.value("screenshot");
    if (parser.isSet("screenshot") && (screenshot.isEmpty() || QFileInfo::exists(screenshot))) {
        qCritical() << "Screenshot needs a new file path; existing files are never overwritten";
        return 2;
    }
    QQuickStyle::setStyle("Basic");
    const bool online = !parser.isSet("offline") && !parser.isSet("smoke-test") && !parser.isSet("screenshot");
    std::unique_ptr<SettingsActivation> activation;
    if (online) {
        activation = std::make_unique<SettingsActivation>(QDBusConnection::sessionBus());
        const auto claim = activation->claim();
        if (claim == SettingsActivation::Claim::Forwarded) return 0;
        if (claim == SettingsActivation::Claim::Failed) {
            qCritical() << "Cannot own or activate the OpenKey settings session service";
            return 1;
        }
    }
    std::unique_ptr<UiPreferences> preferences;
    if (online) {
        preferences = std::make_unique<UiPreferences>(
            QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)).filePath("openkey/ui.ini"));
        preferences->load();
    }
    SettingsOverview overview(qEnvironmentVariable("XDG_RUNTIME_DIR"),
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation))
            .filePath("fcitx5/conf/openkey.conf"));
    std::unique_ptr<SettingsController> controller;
    std::unique_ptr<HotkeyOverview> hotkeys;
    if (online) {
        controller = std::make_unique<SettingsController>(QDBusConnection::sessionBus());
        hotkeys = std::make_unique<HotkeyOverview>(QDBusConnection::sessionBus());
        QObject::connect(controller.get(), &SettingsController::changed, &overview, [&] {
            if (controller->result() == "confirmed") overview.refresh();
        });
    }
    TextConverter converter;
    AboutInfo about;
    ClipboardBridge clipboard;
    QQmlApplicationEngine engine;
    bool warnings = false;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
        [&](const QList<QQmlError>&) { warnings = true; });
    engine.setInitialProperties({{"themeMode", parser.isSet("theme") || !preferences ? parser.value("theme") : preferences->themeMode()},
                                 {"sidebarExpanded", preferences ? preferences->sidebarExpanded() : true},
                                 {"uiPreferences", QVariant::fromValue(preferences.get())},
                                 {"pageIndex", pages.indexOf(parser.value("page"))},
                                 {"overviewModel", QVariant::fromValue(&overview)},
                                 {"configController", QVariant::fromValue(controller.get())},
                                 {"hotkeyOverview", QVariant::fromValue(hotkeys.get())},
                                 {"textConverter", QVariant::fromValue(&converter)},
                                 {"aboutInfo", QVariant::fromValue(&about)},
                                 {"clipboardBridge", QVariant::fromValue(&clipboard)},
                                 {"uiEnvironment", UiEnvironment::snapshot(
                                     online ? qEnvironmentVariable("XDG_SESSION_TYPE") : QString(),
                                     QGuiApplication::platformName())}});
    engine.load(QUrl("qrc:/qt/qml/OpenKey/Settings/qml/Main.qml"));
    if (engine.rootObjects().isEmpty()) return 1;
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (!window) return 1;
    if (activation) activation->setWindow(window);
    if (online) {
        overview.start();
        controller->reload();
        hotkeys->reload();
    }
    if (parser.isSet("smoke-test") || !screenshot.isEmpty()) {
        QTimer::singleShot(600, &app, [&] {
            if (warnings) { app.exit(1); return; }
            if (!screenshot.isEmpty() && !window->grabWindow().save(screenshot, "PNG")) {
                qCritical() << "Cannot save preview"; app.exit(1); return;
            }
            app.exit(0);
        });
    }
    return app.exec();
}
