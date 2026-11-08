#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QTemporaryFile>
#include <QUrl>
#include <memory>

int runFileConversionWorker(int argc, char** argv);
class FileConversion : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
public:
    explicit FileConversion(QString program, QObject* parent = nullptr, int timeoutMs = 30000);
    ~FileConversion() override;
    bool busy() const { return busy_; }
    QString state() const { return state_; }
    Q_INVOKABLE void start(const QUrl& input, const QUrl& output, const QString& source, const QString& target);
    Q_INVOKABLE void cancel();
signals:
    void changed();
private:
    void complete(int exitCode, QProcess::ExitStatus status);
    void finish(const QString& state);
    QString program_, destination_, state_ = "idle", stopReason_;
    QByteArray originalFingerprint_;
    std::unique_ptr<QTemporaryFile> temporary_;
    QProcess process_;
    QTimer deadline_;
    int timeoutMs_;
    bool busy_ = false;
};
