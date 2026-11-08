#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantList>
#include <functional>

class ApplicationMemoryController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool backupPreviewReady READ backupPreviewReady NOTIFY changed)
    Q_PROPERTY(QVariantList entries READ entries NOTIFY entriesChanged)
    Q_PROPERTY(QString currentApplication READ currentApplication NOTIFY changed)
    Q_PROPERTY(QString identityStatus READ identityStatus NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    explicit ApplicationMemoryController(const QDBusConnection& bus, QObject* parent = nullptr);
    QVariantList entries() const;
    QString currentApplication() const { return currentApplication_; }
    QString identityStatus() const { return identityStatus_; }
    QString result() const { return result_; }
    bool ready() const { return ready_; }
    bool busy() const { return busy_; }
    bool backupPreviewReady() const { return !inspectedJson_.isEmpty(); }
    Q_INVOKABLE QString backupJson() const;
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json, const QString& mode);
    Q_INVOKABLE void cancelBackupPreview();
    Q_INVOKABLE void restoreBackup(const QString& json, const QString& mode);
    Q_INVOKABLE void reload();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void update(const QString& application, const QString& expectedMethod, const QString& method);
    Q_INVOKABLE void remove(const QString& application, const QString& expectedMethod);
signals:
    void changed();
    void entriesChanged();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, const QString& method, const QVariantList& args, Callback callback, bool resolve = false);
    void read(Callback callback);
    bool accept(const QDBusMessage& reply);
    void finish(const QString& result, bool ready = false);
    void edit(const QString& application, const QString& expectedMethod, const QString& method);
    void writeTarget(const QString& baseline, const QString& target);
    void clearPreview();
    void cancelRefresh();
    QString inspectedJson_, inspectedMode_, inspectedBaseline_, inspectedTarget_;
    QDBusConnection bus_;
    QString owner_, encoded_, currentApplication_, identityStatus_ = "unavailable", result_ = "offline";
    bool busy_ = false, ready_ = false, writing_ = false, refreshing_ = false;
    quint64 generation_ = 0;
};
