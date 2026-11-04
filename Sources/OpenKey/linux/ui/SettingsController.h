#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>
#include <functional>

// Fixed OpenKey config API. Never writes files or starts/restarts Fcitx.
class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString runtimeStyle READ runtimeStyle NOTIFY changed)
    Q_PROPERTY(QString runtimeDiagnostics READ runtimeDiagnostics NOTIFY changed)
    Q_PROPERTY(bool spellingSupported READ spellingSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeCheckSpelling READ runtimeCheckSpelling NOTIFY changed)
    Q_PROPERTY(QString runtimeModernOrthography READ runtimeModernOrthography NOTIFY changed)
    Q_PROPERTY(bool quickTelexSupported READ quickTelexSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeQuickTelex READ runtimeQuickTelex NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
public:
    explicit SettingsController(const QDBusConnection& bus, QObject* parent = nullptr);
    QString runtimeStyle() const { return values_.value("InputStyle").toString(); }
    QString runtimeDiagnostics() const { return values_.value("DirectDiagnostics").toString(); }
    bool spellingSupported() const { return values_.contains("CheckSpelling") && values_.contains("UseModernOrthography"); }
    QString runtimeCheckSpelling() const { return values_.value("CheckSpelling").toString(); }
    QString runtimeModernOrthography() const { return values_.value("UseModernOrthography").toString(); }
    bool quickTelexSupported() const { return values_.contains("QuickTelex"); }
    QString runtimeQuickTelex() const { return values_.value("QuickTelex").toString(); }
    QString result() const { return result_; }
    bool busy() const { return busy_; }
    bool ready() const { return ready_; }
    Q_INVOKABLE void reload();
    Q_INVOKABLE void apply(const QString& style);
    Q_INVOKABLE void applyDiagnostics(const QString& value);
    Q_INVOKABLE void applySpelling(const QString& value);
    Q_INVOKABLE void applyModernOrthography(const QString& value);
    Q_INVOKABLE void applyQuickTelex(const QString& value);
signals:
    void changed();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, const QString& method, const QVariantList& arguments,
              Callback callback, bool busMethod = false);
    void resolve(Callback callback);
    void read(Callback callback);
    void finish(const QString& result, bool ready = false);
    void accept(const QString& result, const QVariantMap& values);
    void applyValue(const QString& key, const QString& value);
    static bool decode(const QDBusMessage& reply, QVariantMap& values);
    QDBusConnection bus_;
    QString owner_, result_ = "offline";
    QVariantMap values_;
    bool busy_ = false, ready_ = false, writing_ = false;
    quint64 generation_ = 0;
};
