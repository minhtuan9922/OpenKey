#pragma once

#include <QDBusConnection>
#include <QDBusMessage>
#include <QObject>
#include <QVariantMap>
#include <functional>

// Read-only snapshot of Fcitx's global hotkeys. Loading is explicitly requested.
class HotkeyOverview : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(QVariantMap shortcuts READ shortcuts NOTIFY changed)
public:
    explicit HotkeyOverview(const QDBusConnection& bus, QObject* parent = nullptr);
    QString state() const { return state_; }
    QVariantMap shortcuts() const { return shortcuts_; }
    Q_INVOKABLE void reload();
signals:
    void changed();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, bool resolve, Callback callback);
    void finish(const QString& state, const QVariantMap& shortcuts = {});
    static bool decode(const QDBusMessage& reply, QVariantMap& shortcuts);
    QDBusConnection bus_;
    QString state_ = QStringLiteral("unavailable");
    QVariantMap shortcuts_;
    quint64 generation_ = 0;
};
