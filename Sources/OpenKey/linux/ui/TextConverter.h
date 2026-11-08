#pragma once

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>

// In-memory worker conversion; no external services or I/O. At most one job.
// States: idle (also after clear), running, cancelled, empty, invalid, too-large, converted.
// Options are exact, case-sensitive tokens: none/nfc/nfd and keep/upper/lower.
// Limits are UTF-16 code units: 64 Ki input, 256 Ki output. Unpaired
// surrogates and embedded NUL are invalid (NUL can truncate UI display).
// Every failure clears result. Whitespace-only input is a valid conversion.
class TextConverter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(QString state READ state NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
public:
    static constexpr qsizetype inputLimit = 64 * 1024;
    static constexpr qsizetype outputLimit = 256 * 1024;
    explicit TextConverter(QObject* parent = nullptr, qsizetype maxInput = inputLimit, qsizetype maxOutput = outputLimit);
    ~TextConverter() override;
    QString result() const { return result_; }
    QString state() const { return state_; }
    bool busy() const { return busy_; }
    Q_INVOKABLE void convert(const QString& input, const QString& normalization,
                             const QString& letterCase, bool removeVietnameseMarks);
    Q_INVOKABLE void convertLegacy(const QString& input, const QString& source, const QString& target);
    Q_INVOKABLE void clear();
    Q_INVOKABLE void cancel();
signals:
    void changed();
private:
    void convertInternal(const QString& input, const QString& normalization, const QString& letterCase,
                         bool removeVietnameseMarks, const QString& source, const QString& target, bool legacy);
    void finish(const QString& state, const QString& result = {});
    qsizetype maxInput_, maxOutput_;
    QString result_;
    QString state_ = QStringLiteral("idle");
    bool busy_ = false;
    std::shared_ptr<std::atomic_bool> cancellation_;
};
