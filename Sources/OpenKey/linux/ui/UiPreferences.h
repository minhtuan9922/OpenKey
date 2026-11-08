#pragma once
#include <QObject>
#include <QVariantMap>

// UI-only file, independent from Fcitx configuration. Saves only explicit UI actions.
class UiPreferences : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString themeMode READ themeMode NOTIFY changed)
    Q_PROPERTY(bool sidebarExpanded READ sidebarExpanded NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit UiPreferences(QString file, QObject* parent = nullptr);
    void load();
    QString themeMode() const { return theme_; }
    bool sidebarExpanded() const { return expanded_; }
    QString status() const { return status_; }
    Q_INVOKABLE bool setThemeMode(const QString& theme);
    Q_INVOKABLE bool setSidebarExpanded(bool expanded);
    Q_INVOKABLE QString backupJson(const QString& theme, bool sidebarExpanded) const;
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json) const;
    static QVariantMap inspectBackupJson(const QString& json);
    Q_INVOKABLE bool restoreBackup(const QString& json);
signals:
    void changed();
private:
    void loadInternal(bool notify);
    bool save(const QString& theme, bool expanded, bool preserveOnFailure = true);
    QString file_, theme_ = "system", status_ = "not-read";
    bool expanded_ = true, blocked_ = false;
};
