#pragma once
#include <QObject>
#include <QString>

// Build information for this UI, not a claim about the loaded Fcitx addon.
class AboutInfo : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString sourceVersion READ sourceVersion CONSTANT)
    Q_PROPERTY(QString qtBuildVersion READ qtBuildVersion CONSTANT)
    Q_PROPERTY(QString qtRuntimeVersion READ qtRuntimeVersion CONSTANT)
    Q_PROPERTY(QString licenseText READ licenseText NOTIFY changed)
    Q_PROPERTY(QString licenseState READ licenseState NOTIFY changed)
public:
    explicit AboutInfo(QObject* parent = nullptr);
    QString sourceVersion() const;
    QString qtBuildVersion() const;
    QString qtRuntimeVersion() const;
    QString licenseText() const { return license_; }
    QString licenseState() const { return state_; }
    Q_INVOKABLE void loadLicense();
    Q_INVOKABLE void clearLicense();
signals:
    void changed();
private:
    QString license_, state_ = QStringLiteral("idle");
};
