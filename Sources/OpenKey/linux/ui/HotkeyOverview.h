#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>
#include <functional>

// Fcitx global hotkeys, with explicit validated single-field updates.
class HotkeyOverview : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY changed)
public:
    explicit HotkeyOverview(const QDBusConnection& bus, QObject* parent = nullptr);
    QString state() const { return state_; }
    QVariantMap shortcuts() const { return shortcuts_; }
    Q_INVOKABLE void reload();
    Q_INVOKABLE void apply(const QString& field, const QString& keys);
    Q_INVOKABLE QString backupJson() const;
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json) const;
    static QVariantMap inspectBackupJson(const QString& json);
signals:
    void changed();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, bool resolve, Callback callback);
    void write(const QString& owner, const QVariantMap& patch, Callback callback);
    void finish(const QString& state, const QVariantMap& shortcuts = {});
    static bool decode(const QDBusMessage& reply, QVariantMap& shortcuts);
    QDBusConnection bus_;
    QString owner_;
    QString state_ = QStringLiteral("unavailable");
    QVariantMap shortcuts_;
    quint64 generation_ = 0;
};
