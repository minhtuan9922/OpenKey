// Session service with optional X11 settings hotkey; never an input engine.
#include "FcitxStatus.h"
#include "InputMonitor.h"
#include "TrayItem.h"
#include "X11Hotkey.h"
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
struct Fd {
    int value;
    explicit Fd(int fd) : value(fd) {}
    ~Fd() { if (value >= 0) close(value); }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
bool privateDirectory(const std::string& path, bool missingAllowed) {
    struct stat info{};
    if (lstat(path.c_str(), &info) != 0) {
        if (missingAllowed && errno == ENOENT) return false;
        throw std::runtime_error("Cannot inspect runtime directory");
    }
    require(S_ISDIR(info.st_mode) && info.st_uid == getuid() && (info.st_mode & 0077) == 0,
            "Runtime directory must be an owned directory with private permissions (0700)");
    return true;
}
bool peerIsOwner(int socket) {
    struct ucred credentials{};
    socklen_t size = sizeof(credentials);
    return getsockopt(socket, SOL_SOCKET, SO_PEERCRED, &credentials, &size) == 0 &&
        size == sizeof(credentials) && credentials.uid == getuid();
}
bool sendLine(int socket, const std::string& text) {
    // Protocol messages are small and bounded; do not die on a disconnected client.
    timeval timeout{0, 250000};
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    std::size_t sent = 0;
    while (sent < text.size()) {
        const auto count = send(socket, text.data() + sent, text.size() - sent, MSG_NOSIGNAL);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) return false;
        sent += static_cast<std::size_t>(count);
    }
    return true;
}
std::string readLine(int socket, int timeout = 250) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    std::string result;
    while (result.size() < 32) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) return {};
        pollfd fd{socket, POLLIN, 0};
        const auto ready = poll(&fd, 1, static_cast<int>(remaining));
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) return {};
        char ch;
        const auto count = recv(socket, &ch, 1, 0);
        if (count < 0 && errno == EINTR) continue;
        if (count != 1) return {};
        if (ch == '\n') return result;
        result += ch;
    }
    return {};
}
struct SocketCleanup {
    std::string path;
    ~SocketCleanup() { if (!path.empty()) unlink(path.c_str()); }
};
int execute(const std::string& command, bool enableHotkey) {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    require(runtime && runtime[0] == '/', "XDG_RUNTIME_DIR must name a private absolute runtime directory");
    const bool run = command == "--run" || command == "--run-tray";
    if (!privateDirectory(runtime, !run)) { std::cout << "stopped\n"; return 1; }
    const std::string directory = std::string(runtime) + "/openkey";
    if (run) require(mkdir(directory.c_str(), 0700) == 0 || errno == EEXIST, "Cannot create service directory");
    if (!privateDirectory(directory, !run)) { std::cout << "stopped\n"; return 1; }
    const std::string path = directory + "/session.sock";
    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    require(path.size() < sizeof(address.sun_path), "Runtime socket path is too long");
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);

    if (!run) {
        Fd socket(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
        require(socket.value >= 0, "Cannot create client socket");
        if (connect(socket.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
            if (errno == ENOENT || errno == ECONNREFUSED) { std::cout << "stopped\n"; return 1; }
            throw std::runtime_error("Cannot connect to service");
        }
        require(peerIsOwner(socket.value), "Service belongs to another user");
        const bool input = command == "--input-status";
        const bool tray = command == "--tray-status";
        const bool hotkey = command == "--hotkey-status";
        require(sendLine(socket.value, hotkey ? "hotkey-status\n" : tray ? "tray-status\n" : input ? "input-status\n" :
            (command == "--status" ? "status\n" : "quit\n")), "Cannot send request");
        const auto response = readLine(socket.value, input ? 1000 : 250);
        const bool knownInput = response == "telex" || response == "vni" || response == "english" ||
            response == "inactive" || response == "other" || response == "unknown" || response == "unavailable";
        const bool knownTray = response == "disabled" || response == "unavailable" ||
            response == "waiting-host" || response == "registered";
        const bool knownHotkey = response == "disabled" || response == "unsupported-session" || response == "not-built" ||
            response == "active" || response == "conflict" || response == "unavailable" || response == "error";
        require(hotkey ? knownHotkey : tray ? knownTray : input ? knownInput : response == (command == "--status" ? "running" : "stopping"),
                "Invalid service response");
        std::cout << response << '\n';
        return 0;
    }

    Fd lock(open((directory + "/session.lock").c_str(), O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600));
    require(lock.value >= 0, "Cannot open service lock");
    struct stat lockInfo{};
    require(fstat(lock.value, &lockInfo) == 0 && S_ISREG(lockInfo.st_mode) &&
                lockInfo.st_uid == getuid() && lockInfo.st_nlink == 1 && (lockInfo.st_mode & 0077) == 0,
            "Unsafe service lock");
    if (flock(lock.value, LOCK_EX | LOCK_NB) != 0) {
        require(errno == EWOULDBLOCK, "Cannot acquire service lock");
        std::cerr << "OpenKey service already running\n";
        return 3;
    }
    // Only the lock owner may remove a stale IPC socket. Never remove a regular file.
    struct stat oldSocket{};
    if (lstat(path.c_str(), &oldSocket) == 0) {
        require(S_ISSOCK(oldSocket.st_mode) && oldSocket.st_uid == getuid(), "Unsafe stale socket path");
        require(unlink(path.c_str()) == 0, "Cannot remove stale socket");
    } else require(errno == ENOENT, "Cannot inspect socket path");
    Fd server(socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
    require(server.value >= 0, "Cannot create service socket");
    require(bind(server.value, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0, "Cannot bind service socket");
    SocketCleanup cleanup{path};
    require(chmod(path.c_str(), 0600) == 0 && listen(server.value, 8) == 0, "Cannot listen on service socket");
    struct sigaction action{};
    action.sa_handler = stop;
    sigemptyset(&action.sa_mask);
    require(sigaction(SIGINT, &action, nullptr) == 0 && sigaction(SIGTERM, &action, nullptr) == 0,
            "Cannot install signal handlers");
    openkey::InputMonitor inputMonitor;
    openkey::TrayItem tray(command == "--run-tray");
    openkey::X11Hotkey hotkey(enableHotkey, openkey::currentDesktopSession());
    std::cout << "OpenKey session service ready\n" << std::flush;
    while (!stopping) {
        if (hotkey.consume()) tray.requestConfiguration();
        tray.tick(inputMonitor.snapshot(), hotkey.status());
        if (tray.quitRequested()) break;
        pollfd fd{server.value, POLLIN, 0};
        const auto ready = poll(&fd, 1, 100);
        if (ready < 0 && errno == EINTR) continue;
        require(ready >= 0, "Service poll failed");
        if (!ready) continue;
        Fd client(accept4(server.value, nullptr, nullptr, SOCK_CLOEXEC));
        if (client.value < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        require(client.value >= 0, "Cannot accept client");
        if (!peerIsOwner(client.value)) continue;
        const auto request = readLine(client.value);
        if (request == "status") sendLine(client.value, "running\n");
        else if (request == "tray-status") sendLine(client.value, std::string(tray.registrationState()) + "\n");
        else if (request == "hotkey-status") sendLine(client.value, std::string(openkey::hotkeyStatusName(hotkey.status())) + "\n");
        else if (request == "input-status")
            sendLine(client.value, std::string(openkey::statusName(inputMonitor.snapshot())) + "\n");
        else if (request == "quit") { sendLine(client.value, "stopping\n"); stopping = 1; }
        else sendLine(client.value, "error\n");
    }
    return 0;
}
} // namespace
int main(int argc, char** argv) {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "openkey-service --run | --run-tray [--x11-settings-hotkey] | --status | --input-status | --tray-status | --hotkey-status | --quit\n"
                     "Optional status tray with Telex/VNI/English, Fcitx settings and tray-only Quit. X11 hotkey and autostart are opt-in.\n";
        return 0;
    }
    const bool enableHotkey = argc == 3 && std::string(argv[1]) == "--run-tray" && std::string(argv[2]) == "--x11-settings-hotkey";
    if ((argc != 2 && !enableHotkey) || (std::string(argv[1]) != "--run" && std::string(argv[1]) != "--run-tray" && std::string(argv[1]) != "--status" &&
                      std::string(argv[1]) != "--quit" && std::string(argv[1]) != "--input-status" && std::string(argv[1]) != "--tray-status" &&
                      std::string(argv[1]) != "--hotkey-status")) {
        std::cerr << "Usage: openkey-service --run | --run-tray [--x11-settings-hotkey] | --status | --input-status | --tray-status | --hotkey-status | --quit\n";
        return 2;
    }
    try { return execute(argv[1], enableHotkey); }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 2; }
}
