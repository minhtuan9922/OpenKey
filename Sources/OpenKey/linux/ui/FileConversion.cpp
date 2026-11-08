#include "FileConversion.h"
#include "TextConverter.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
namespace {
constexpr qint64 inputBytes = 16 * 1024 * 1024, outputBytes = 64 * 1024 * 1024;
const QStringList encodings{"unicode", "compound", "vni", "tcvn3", "cp1258"};
QString selectedPath(const QUrl& url) {
    if (!url.isValid() || !url.isLocalFile() || !url.host().isEmpty() || url.hasQuery() || url.hasFragment()) return {};
    const auto path = url.toLocalFile();
    return QFileInfo(path).isAbsolute() && !path.contains(QChar(0)) ? path : QString();
}
QByteArray fingerprint(const QString& path) {
    struct stat info{};
    if (::lstat(QFile::encodeName(path).constData(), &info) != 0) return QFileInfo::exists(path) ? QByteArray("error") : QByteArray("absent");
    if (!S_ISREG(info.st_mode)) return "error";
    return QByteArray::number(info.st_dev) + ':' + QByteArray::number(info.st_ino) + ':' + QByteArray::number(info.st_size) + ':' +
        QByteArray::number(info.st_mtim.tv_sec) + ':' + QByteArray::number(info.st_mtim.tv_nsec) + ':' +
        QByteArray::number(info.st_ctim.tv_sec) + ':' + QByteArray::number(info.st_ctim.tv_nsec);
}
}
FileConversion::FileConversion(QString program, QObject* parent, int timeoutMs)
    : QObject(parent), program_(std::move(program)), timeoutMs_(timeoutMs) {
    deadline_.setSingleShot(true);
    connect(&deadline_, &QTimer::timeout, this, [this] { stopReason_ = "timeout"; process_.kill(); });
    connect(&process_, &QProcess::finished, this, &FileConversion::complete);
    connect(&process_, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) finish("failed");
    });
}
FileConversion::~FileConversion() { if (process_.state() != QProcess::NotRunning) { process_.kill(); process_.waitForFinished(1000); } }
void FileConversion::finish(const QString& state) {
    deadline_.stop(); temporary_.reset(); stopReason_.clear();
    busy_ = false; state_ = state; emit changed();
}
void FileConversion::start(const QUrl& input, const QUrl& output, const QString& source, const QString& target) {
    if (busy_) return;
    const auto from = selectedPath(input), to = selectedPath(output);
    if (from.isEmpty() || to.isEmpty() || !encodings.contains(source) || !encodings.contains(target) || from == to ||
        (QFileInfo::exists(to) && QFileInfo(from).canonicalFilePath() == QFileInfo(to).canonicalFilePath())) { finish("invalid"); return; }
    const QFileInfo file(from);
    if (file.isSymLink() || !file.isFile() || file.size() > inputBytes) { finish("invalid-input"); return; }
    originalFingerprint_ = fingerprint(to);
    if (originalFingerprint_ == "error") { finish("invalid-output"); return; }
    temporary_ = std::make_unique<QTemporaryFile>(QFileInfo(to).dir().filePath(".openkey-convert-XXXXXX"));
    if (!temporary_->open()) { finish("unwritable"); return; }
    temporary_->close(); destination_ = to;
    busy_ = true; state_ = "running"; stopReason_.clear(); emit changed();
    process_.setProgram(program_);
    process_.setArguments({"--file-worker", from, temporary_->fileName(), source, target});
    process_.start(); deadline_.start(timeoutMs_);
}
void FileConversion::cancel() {
    if (!busy_ || !stopReason_.isEmpty()) return;
    stopReason_ = "cancelled"; state_ = "cancelling"; emit changed(); process_.kill();
}
void FileConversion::complete(int exitCode, QProcess::ExitStatus status) {
    if (!busy_) return;
    if (!stopReason_.isEmpty()) { const auto reason = stopReason_; finish(reason); return; }
    const auto response = process_.readAllStandardOutput();
    process_.readAllStandardError();
    if (status != QProcess::NormalExit || exitCode != 0 || response.size() > 4096 ||
        QJsonDocument::fromJson(response).object() != QJsonObject{{"ok", true}}) { finish("failed"); return; }
    if (!temporary_ || QFileInfo(temporary_->fileName()).isSymLink() || !QFileInfo(temporary_->fileName()).isFile() ||
        QFileInfo(temporary_->fileName()).size() > outputBytes) { finish("failed"); return; }
    if (fingerprint(destination_) != originalFingerprint_) { finish("conflict"); return; }
    // Temporary output is on the destination filesystem. Publish only after a
    // successful worker exit and a final conflict check, never after cancel/timeout.
    const auto staged = QFile::encodeName(temporary_->fileName()), destination = QFile::encodeName(destination_);
    if (originalFingerprint_ == "absent") {
        // link is atomic and refuses a newly appeared destination. The owned
        // staging link is removed by QTemporaryFile after publication.
        if (::link(staged.constData(), destination.constData()) != 0) { finish(errno == EEXIST ? "conflict" : "unwritable"); return; }
    } else if (::rename(staged.constData(), destination.constData()) != 0) { finish("unwritable"); return; }
    finish("converted");
}
int runFileConversionWorker(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    const auto args = app.arguments();
    if (args.size() != 6 || args[1] != "--file-worker" || !encodings.contains(args[4]) || !encodings.contains(args[5])) return 2;
    const int fd = ::open(QFile::encodeName(args[2]).constData(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) return 2;
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > inputBytes) { ::close(fd); return 2; }
    QFile input;
    if (!input.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) { ::close(fd); return 2; }
    auto bytes = input.read(inputBytes + 1);
    if (input.error() != QFileDevice::NoError || bytes.size() > inputBytes) return 2;
    if (bytes.startsWith(QByteArray::fromHex("efbbbf"))) bytes.remove(0, 3);
    const auto text = QString::fromUtf8(bytes);
    if (text.toUtf8() != bytes || text.contains(QChar(0))) return 2;
    TextConverter converter(nullptr, inputBytes, outputBytes);
    const auto save = [&](const QString& result) {
        const auto data = result.toUtf8();
        if (data.size() > outputBytes) { app.exit(2); return; }
        // Write only the parent's already-created temporary file. No second
        // staging pathname can be orphaned if the parent kills this worker.
        const int outputFd = ::open(QFile::encodeName(args[3]).constData(), O_WRONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
        if (outputFd < 0) { app.exit(2); return; }
        struct stat outputInfo{};
        if (::fstat(outputFd, &outputInfo) != 0 || !S_ISREG(outputInfo.st_mode) || ::ftruncate(outputFd, 0) != 0) { ::close(outputFd); app.exit(2); return; }
        QFile output;
        if (!output.open(outputFd, QIODevice::WriteOnly, QFileDevice::AutoCloseHandle)) { ::close(outputFd); app.exit(2); return; }
        if (output.write(data) != data.size() || !output.flush() || ::fsync(outputFd) != 0) { app.exit(2); return; }
        QFile out;
        if (!out.open(stdout, QIODevice::WriteOnly) || out.write("{\"ok\":true}") != 11 || !out.flush()) { app.exit(2); return; }
        app.exit(0);
    };
    QObject::connect(&converter, &TextConverter::changed, &app, [&] {
        if (converter.state() == "converted") save(converter.result());
        else if (converter.state() == "empty") save({});
        else if (!converter.busy() && converter.state() != "idle") app.exit(2);
    });
    QTimer::singleShot(0, &app, [&] { converter.convertLegacy(text, args[4], args[5]); });
    return app.exec();
}
