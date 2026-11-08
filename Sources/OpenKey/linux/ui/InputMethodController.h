#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QStringList>
#include <functional>

// Explicit Fcitx IM selection. Never edits the user's input-method group or starts Fcitx.
class InputMethodController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString current READ current NOTIFY changed)
    Q_PROPERTY(QStringList methods READ methods NOTIFY changed)
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
public:
    explicit InputMethodController(const QDBusConnection& bus, QObject* parent = nullptr);
    QString current() const { return snapshot_.current; }
    QStringList methods() const { return snapshot_.methods; }
    bool active() const { return snapshot_.state == 2; }
    bool ready() const { return ready_; }
    bool busy() const { return busy_; }
    QString result() const { return result_; }
    Q_INVOKABLE QString backupJson() const;
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json) const;
    static QString methodFromBackup(const QString& json);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void select(const QString& method);
signals:
    void changed();
private:
    struct Snapshot { QString group, current; QStringList methods; int state = 0; };
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, const QString& method, const QVariantList& args, Callback callback, bool resolve = false);
    void resolve(Callback callback);
    void read(std::function<void(const Snapshot&)> callback);
    void finish(const QString& result, bool ready = false);
    void accept(const QString& result, const Snapshot& snapshot);
    QDBusConnection bus_;
    Snapshot snapshot_;
    QString owner_, result_ = "offline";
    bool ready_ = false, busy_ = false, writing_ = false;
    quint64 generation_ = 0;
};
