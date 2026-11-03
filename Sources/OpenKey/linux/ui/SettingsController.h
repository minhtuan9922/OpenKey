#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <functional>

// Fixed OpenKey config API. Never writes files or starts/restarts Fcitx.
class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString runtimeStyle READ runtimeStyle NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
public:
    explicit SettingsController(const QDBusConnection& bus, QObject* parent = nullptr);
    QString runtimeStyle() const { return style_; }
    QString result() const { return result_; }
    bool busy() const { return busy_; }
    bool ready() const { return ready_; }
    Q_INVOKABLE void reload();
    Q_INVOKABLE void apply(const QString& style);
signals:
    void changed();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, const QString& method, const QVariantList& arguments,
              Callback callback, bool busMethod = false);
    void resolve(Callback callback);
    void read(Callback callback);
    void finish(const QString& result, bool ready = false);
    static bool decode(const QDBusMessage& reply, QString& style);
    QDBusConnection bus_;
    QString owner_, style_, result_ = "offline";
    bool busy_ = false, ready_ = false, writing_ = false;
    quint64 generation_ = 0;
};
