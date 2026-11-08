#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QPointer>
#include <QQuickWindow>
#include <QStringList>

class SettingsActivation : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.openkey.Settings1")
public:
    enum class Claim { Primary, Forwarded, Failed };
    explicit SettingsActivation(const QDBusConnection& bus, QObject* parent = nullptr)
        : QObject(parent), bus_(bus) {}
    ~SettingsActivation() override;
    Claim claim();
    void setWindow(QQuickWindow* window);
    static const QStringList& pages();
public slots:
    Q_SCRIPTABLE void Show();
    Q_SCRIPTABLE bool ShowPage(const QString& page);
signals:
    void showRequested();
private:
    QDBusConnection bus_;
    QPointer<QQuickWindow> window_;
    bool owns_ = false;
    int requestedPage_ = -1;
};
