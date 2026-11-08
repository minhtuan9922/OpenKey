#include "BackupFiles.h"
#include "ApplicationMemoryBackup.h"
#include "InputConfigBackup.h"
#include "UiPreferences.h"
#include "MacroModel.h"
#include "InputMethodController.h"
#include "HotkeyOverview.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace {
QString selectedPath(const QUrl& url) {
    if (!url.isValid() || !url.isLocalFile() || !url.host().isEmpty() || url.hasQuery() || url.hasFragment()) return {};
    const auto path = url.toLocalFile();
    return QFileInfo(path).isAbsolute() && !path.contains(QChar(0)) ? path : QString();
}
QVariantMap failed(const QString& status) { return {{"ok", false}, {"status", status}}; }
}

bool BackupFiles::validBackup(const QString& text) const {
    QVariantMap patch;
    return InputConfigBackup::decode(text, patch);
}

bool BackupFiles::validUiBackup(const QString& text) const {
    return UiPreferences::inspectBackupJson(text).value("ok").toBool();
}
QVariantMap BackupFiles::load(const QUrl& url) const { return loadType(url, 0); }
QVariantMap BackupFiles::loadUi(const QUrl& url) const { return loadType(url, 1); }
QString BackupFiles::save(const QUrl& url, const QString& text) const { return saveType(url, text, 0); }
QString BackupFiles::saveUi(const QUrl& url, const QString& text) const { return saveType(url, text, 1); }

bool BackupFiles::validMacroBackup(const QString& text) const { MacroModel model; return model.importJson(text); }
QVariantMap BackupFiles::loadMacros(const QUrl& url) const { return loadType(url, 2); }
QString BackupFiles::saveMacros(const QUrl& url, const QString& text) const { return saveType(url, text, 2); }

bool BackupFiles::validMethodBackup(const QString& text) const { return !InputMethodController::methodFromBackup(text).isEmpty(); }
bool BackupFiles::validHotkeyBackup(const QString& text) const { return HotkeyOverview::inspectBackupJson(text).value("ok").toBool(); }
QVariantMap BackupFiles::loadMethod(const QUrl& url) const { return loadType(url, 3); }
QVariantMap BackupFiles::loadHotkeys(const QUrl& url) const { return loadType(url, 4); }
QString BackupFiles::saveMethod(const QUrl& url, const QString& text) const { return saveType(url, text, 3); }
QString BackupFiles::saveHotkeys(const QUrl& url, const QString& text) const { return saveType(url, text, 4); }
bool BackupFiles::validApplicationMemoryBackup(const QString& text) const {
    openkey::MacroTable::Entries entries;
    return ApplicationMemoryBackup::decode(text, entries);
}
QVariantMap BackupFiles::loadApplicationMemory(const QUrl& url) const { return loadType(url, 5); }
QString BackupFiles::saveApplicationMemory(const QUrl& url, const QString& text) const { return saveType(url, text, 5); }
bool BackupFiles::validType(const QString& text, int kind) const {
    return kind == 0 ? validBackup(text) : kind == 1 ? validUiBackup(text) : kind == 2 ? validMacroBackup(text) :
        kind == 3 ? validMethodBackup(text) : kind == 4 ? validHotkeyBackup(text) : kind == 5 && validApplicationMemoryBackup(text);
}
QVariantMap BackupFiles::loadType(const QUrl& url, int kind) const {
    const qint64 maxBytes = kind == 2 ? 524288 : (kind == 4 || kind == 5) ? 131072 : 16384;
    const auto path = selectedPath(url);
    if (path.isEmpty()) return failed("invalid-path");
    // Refuse special files without blocking on a FIFO/device. Bound both bytes
    // and decoded text; never return arbitrary file data to the QML editor.
    const int fd = ::open(QFile::encodeName(path).constData(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) return failed("unreadable");
    struct stat info{};
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > maxBytes) {
        ::close(fd);
        return failed("invalid-file");
    }
    QFile file;
    if (!file.open(fd, QIODevice::ReadOnly, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        return failed("unreadable");
    }
    const auto bytes = file.read(maxBytes + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() > maxBytes) return failed("invalid-file");
    const auto utf8 = bytes.startsWith(QByteArray::fromHex("efbbbf")) ? bytes.mid(3) : bytes;
    const auto text = QString::fromUtf8(utf8);
    // Roundtrip catches replacement characters and an incomplete final UTF-8
    // sequence as well. A single optional UTF-8 BOM is accepted.
    if (text.toUtf8() != utf8 || !validType(text, kind)) return failed("invalid-json");
    return {{"ok", true}, {"status", "loaded"}, {"text", text}};
}

QString BackupFiles::saveType(const QUrl& url, const QString& text, int kind) const {
    if (!validType(text, kind)) return "invalid-json";
    const auto path = selectedPath(url);
    if (path.isEmpty()) return "invalid-path";
    const QFileInfo target(path);
    if (target.isSymLink() || (target.exists() && !target.isFile())) return "invalid-file";
    QSaveFile file(path);
    // Keep atomic replacement; never truncate the original as a fallback.
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return "unwritable";
    const auto bytes = text.toUtf8();
    if (file.write(bytes) != bytes.size()) { file.cancelWriting(); return "unwritable"; }
    return file.commit() ? "saved" : "unwritable";
}
