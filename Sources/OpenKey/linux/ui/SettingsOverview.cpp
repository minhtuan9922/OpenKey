#include "SettingsOverview.h"
#include <QDir>
#include <QFile>
#include <QSet>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace {
bool privateDirectory(const QString& path) {
    struct stat info{};
    return QDir::isAbsolutePath(path) &&
        ::lstat(QFile::encodeName(path).constData(), &info) == 0 &&
        S_ISDIR(info.st_mode) && info.st_uid == getuid() && (info.st_mode & 0077) == 0;
}
bool privateSocket(const QString& path) {
    struct stat info{};
    return ::lstat(QFile::encodeName(path).constData(), &info) == 0 &&
        S_ISSOCK(info.st_mode) && info.st_uid == getuid() && (info.st_mode & 0077) == 0;
}
bool ownerPeer(qintptr fd) {
    struct ucred credentials{};
    socklen_t size = sizeof(credentials);
    return ::getsockopt(static_cast<int>(fd), SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0 &&
        size == sizeof(credentials) && credentials.uid == getuid();
}
}

SettingsOverview::SettingsOverview(QString runtimeDirectory, QString configFile, QObject* parent)
    : QObject(parent), runtimeDirectory_(std::move(runtimeDirectory)), configFile_(std::move(configFile)) {
    poll_.setInterval(1000);
    deadline_.setSingleShot(true);
    deadline_.setInterval(600);
    socket_.setReadBufferSize(64);
    connect(&poll_, &QTimer::timeout, this, &SettingsOverview::refresh);
    connect(&deadline_, &QTimer::timeout, this, [this] { finish("timeout"); });
    connect(&socket_, &QLocalSocket::connected, this, [this] {
        if (!pending_) return;
        if (!ownerPeer(socket_.socketDescriptor())) { finish("unavailable"); return; }
        if (socket_.write("input-status\n") != 13) finish("unavailable");
    });
    connect(&socket_, &QLocalSocket::readyRead, this, &SettingsOverview::consume);
    connect(&socket_, &QLocalSocket::disconnected, this, [this] {
        if (pending_) { consume(); if (pending_) finish("unavailable"); }
    });
    connect(&socket_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError error) {
        // Peer-close may arrive before the final readyRead notification.
        if (error == QLocalSocket::PeerClosedError) consume();
        if (pending_) finish("unavailable");
    });
}

SettingsOverview::~SettingsOverview() {
    poll_.stop();
    deadline_.stop();
    socket_.disconnect(this);
    socket_.abort();
}

void SettingsOverview::start() { refresh(); poll_.start(); }

void SettingsOverview::refresh() {
    readConfig();
    if (pending_) return;
    const auto directory = runtimeDirectory_ + "/openkey";
    const auto path = directory + "/session.sock";
    if (!privateDirectory(runtimeDirectory_) || !privateDirectory(directory) || !privateSocket(path)) {
        finish("unavailable");
        return;
    }
    pending_ = true;
    response_.clear();
    if (serviceState_ != "connected" && serviceState_ != "connecting") {
        serviceState_ = "connecting";
        emit changed();
    }
    deadline_.start();
    socket_.connectToServer(path, QIODevice::ReadWrite);
}

void SettingsOverview::consume() {
    if (!pending_) return;
    response_ += socket_.readAll();
    if (response_.size() > 32) { finish("protocol-error"); return; }
    if (!response_.contains('\n')) return;
    static const QSet<QByteArray> allowed{"telex", "vni", "simple-telex1", "simple-telex2", "english", "inactive", "other", "unknown", "unavailable"};
    // Exactly one bounded line; never display arbitrary server content.
    const auto value = response_.chopped(1);
    if (!response_.endsWith('\n') || !allowed.contains(value)) { finish("protocol-error"); return; }
    finish("connected", QString::fromLatin1(value));
}

void SettingsOverview::finish(const QString& state, const QString& input) {
    pending_ = false;
    deadline_.stop();
    socket_.abort();
    response_.clear();
    if (serviceState_ == state && inputStatus_ == input) return;
    serviceState_ = state;
    inputStatus_ = input;
    emit changed();
}

void SettingsOverview::readConfig() {
    QString state = "unreadable", style = "unknown", diagnostics = "unknown";
    // Do not open FIFOs/devices or follow a final symlink. Never create a file.
    const int fd = ::open(QFile::encodeName(configFile_).constData(), O_RDONLY | O_CLOEXEC | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT) state = "missing";
    } else {
        struct stat info{};
        if (::fstat(fd, &info) == 0 && S_ISREG(info.st_mode) && info.st_size <= 65536) {
            QFile file;
            if (file.open(fd, QIODevice::ReadOnly, QFileDevice::DontCloseHandle)) {
                const auto data = file.read(65537);
                if (file.error() == QFileDevice::NoError && data.size() <= 65536 && !data.contains('\0')) {
                    state = "read";
                    QSet<QByteArray> seen;
                    bool root = true;
                    for (auto line : data.split('\n')) {
                        line = line.trimmed();
                        if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) continue;
                        if (line.startsWith('[')) { root = false; continue; }
                        if (!root) continue;
                        const auto equals = line.indexOf('=');
                        const auto key = (equals < 0 ? line : line.left(equals)).trimmed();
                        if (key != "InputStyle" && key != "DirectDiagnostics") continue;
                        if (equals < 0 || seen.contains(key)) { state = "invalid"; break; }
                        seen.insert(key);
                        const auto value = line.mid(equals + 1).trimmed();
                        if (key == "InputStyle") {
                            if (value == "Preedit") style = "preedit";
                            else if (value == "Direct") style = "direct";
                            else if (value == "CommitOnly") style = "commit-only";
                            else if (value == "NativeDirect") style = "native-direct";
                            else { state = "invalid"; break; }
                        } else {
                            if (value == "True") diagnostics = "enabled";
                            else if (value == "False") diagnostics = "disabled";
                            else { state = "invalid"; break; }
                        }
                    }
                } else state = "invalid";
            }
        }
        ::close(fd);
    }
    if (state != "read") { style = "unknown"; diagnostics = "unknown"; }
    if (configState_ == state && inputStyle_ == style && diagnostics_ == diagnostics) return;
    configState_ = state;
    inputStyle_ = style;
    diagnostics_ = diagnostics;
    emit changed();
}
