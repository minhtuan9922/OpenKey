#pragma once

#include <QString>
#include <QVariantMap>

class UiEnvironment {
public:
    // Exact allowlist matches only; each supplied value is classified independently.
    static QVariantMap snapshot(const QString& session, const QString& platform);
};
