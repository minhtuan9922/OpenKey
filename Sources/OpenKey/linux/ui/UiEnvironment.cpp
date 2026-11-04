#include "UiEnvironment.h"

QVariantMap UiEnvironment::snapshot(const QString& session, const QString& platform) {
    const bool knownSession = session == QStringLiteral("x11") ||
        session == QStringLiteral("wayland") || session == QStringLiteral("tty");
    const bool knownPlatform = platform == QStringLiteral("xcb") ||
        platform == QStringLiteral("wayland") || platform == QStringLiteral("wayland-egl") ||
        platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal") ||
        platform == QStringLiteral("vnc") || platform == QStringLiteral("eglfs") ||
        platform == QStringLiteral("linuxfb");

    return {{QStringLiteral("sessionType"), knownSession ? session : QStringLiteral("unknown")},
            {QStringLiteral("platformName"), knownPlatform ? platform : QStringLiteral("unknown")}};
}
