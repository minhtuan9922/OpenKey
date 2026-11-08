#pragma once
#include <QObject>
#include <QUrl>
#include <QVariantMap>

// Explicit file-picker actions only. Never applies settings or reads clipboard.
class BackupFiles : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    Q_INVOKABLE bool validBackup(const QString& text) const;
    Q_INVOKABLE QVariantMap load(const QUrl& url) const;
    Q_INVOKABLE QString save(const QUrl& url, const QString& text) const;
    Q_INVOKABLE bool validUiBackup(const QString& text) const;
    Q_INVOKABLE QVariantMap loadUi(const QUrl& url) const;
    Q_INVOKABLE QString saveUi(const QUrl& url, const QString& text) const;
    Q_INVOKABLE bool validMacroBackup(const QString& text) const;
    Q_INVOKABLE QVariantMap loadMacros(const QUrl& url) const;
    Q_INVOKABLE QString saveMacros(const QUrl& url, const QString& text) const;
    Q_INVOKABLE bool validMethodBackup(const QString& text) const;
    Q_INVOKABLE QVariantMap loadMethod(const QUrl& url) const;
    Q_INVOKABLE QString saveMethod(const QUrl& url, const QString& text) const;
    Q_INVOKABLE bool validHotkeyBackup(const QString& text) const;
    Q_INVOKABLE QVariantMap loadHotkeys(const QUrl& url) const;
    Q_INVOKABLE QString saveHotkeys(const QUrl& url, const QString& text) const;
private:
    bool validType(const QString& text, int kind) const;
    QVariantMap loadType(const QUrl& url, int kind) const;
    QString saveType(const QUrl& url, const QString& text, int kind) const;
};
