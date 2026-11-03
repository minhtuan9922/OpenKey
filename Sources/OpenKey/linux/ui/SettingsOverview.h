#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>

// Read-only view model. No input-method switching, activation, config writes,
// subprocesses or document/clipboard access.
class SettingsOverview : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString inputStatus READ inputStatus NOTIFY changed)
    Q_PROPERTY(QString serviceState READ serviceState NOTIFY changed)
    Q_PROPERTY(QString configState READ configState NOTIFY changed)
    Q_PROPERTY(QString inputStyle READ inputStyle NOTIFY changed)
    Q_PROPERTY(QString diagnostics READ diagnostics NOTIFY changed)
public:
    SettingsOverview(QString runtimeDirectory, QString configFile, QObject* parent = nullptr);
    ~SettingsOverview() override;
    void start();
    Q_INVOKABLE void refresh();
    QString inputStatus() const { return inputStatus_; }
    QString serviceState() const { return serviceState_; }
    QString configState() const { return configState_; }
    QString inputStyle() const { return inputStyle_; }
    QString diagnostics() const { return diagnostics_; }
signals:
    void changed();
private:
    void readConfig();
    void finish(const QString& state, const QString& input = QStringLiteral("unknown"));
    void consume();
    QString runtimeDirectory_, configFile_;
    QString inputStatus_ = "unknown", serviceState_ = "disabled";
    QString configState_ = "not-read", inputStyle_ = "unknown", diagnostics_ = "unknown";
    QLocalSocket socket_;
    QTimer poll_, deadline_;
    QByteArray response_;
    bool pending_ = false;
};
