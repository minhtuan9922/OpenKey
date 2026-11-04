#include "AboutInfo.h"
#include <QFile>
#include <QResource>

#ifndef OPENKEY_UI_VERSION
#error OPENKEY_UI_VERSION must be set from the CMake project version
#endif

static void initializeAboutResources() {
    Q_INIT_RESOURCE(openkey_about);
}

AboutInfo::AboutInfo(QObject* parent) : QObject(parent) {
    static const bool initialized = (initializeAboutResources(), true);
    Q_UNUSED(initialized);
}
QString AboutInfo::sourceVersion() const { return QStringLiteral(OPENKEY_UI_VERSION); }
QString AboutInfo::qtBuildVersion() const { return QStringLiteral(QT_VERSION_STR); }
QString AboutInfo::qtRuntimeVersion() const { return QString::fromLatin1(qVersion()); }
void AboutInfo::loadLicense() {
    if (state_ == "ready") return;
    QFile file(QStringLiteral(":/openkey/LICENSE"));
    if (!file.open(QIODevice::ReadOnly) || file.size() > 128 * 1024) {
        license_.clear();
        state_ = QStringLiteral("error");
    } else {
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError || bytes.isEmpty()) {
            license_.clear();
            state_ = QStringLiteral("error");
        } else {
            license_ = QString::fromUtf8(bytes);
            state_ = QStringLiteral("ready");
        }
    }
    emit changed();
}
void AboutInfo::clearLicense() {
    license_.clear();
    state_ = QStringLiteral("idle");
    emit changed();
}
