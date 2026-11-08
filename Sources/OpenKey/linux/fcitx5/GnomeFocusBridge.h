#pragma once
#include <fcitx-utils/dbus/bus.h>
#include <fcitx-utils/dbus/matchrule.h>
#include <fcitx-utils/event.h>
#include <functional>
#include <optional>
#include <string>
#include <algorithm>

namespace openkey {
struct GnomeFocus {
    std::string owner, session, app;
    uint32_t serial = 0;
    bool operator==(const GnomeFocus&) const = default;
    std::string key() const { return "gnome:" + app; }
    static bool validApp(const std::string& app) {
        return app.size() > 8 && app.size() <= 120 && app.ends_with(".desktop") &&
            app.front() != '.' && std::all_of(app.begin(), app.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.';
            });
    }
};
// Optional, lazy session connection. Never activates GNOME Shell or blocks a key.
class GnomeFocusBridge {
public:
    using Callback = std::function<void(std::optional<GnomeFocus>)>;
    GnomeFocusBridge(fcitx::EventLoop& loop, std::function<void()> invalidated)
        : loop_(loop), invalidated_(std::move(invalidated)) {}
    const std::string& status() const { return status_; }
    void cancel() { ++generation_; request_.reset(); ownerRequest_.reset(); }
    void resolve(Callback callback) {
        cancel();
        status_ = "waiting";
        if (!connect()) { status_ = "unavailable"; callback({}); return; }
        const auto generation = generation_;
        ownerRequest_ = bus_->serviceOwnerAsync("org.gnome.Shell", 150000,
            [this, generation, callback = std::make_shared<Callback>(std::move(callback))](fcitx::dbus::Message& reply) mutable {
                if (generation != generation_) return true;
                std::string owner;
                if (reply.type() != fcitx::dbus::MessageType::Reply || reply.signature() != "s") { status_ = "unavailable"; (*callback)({}); return true; }
                reply >> owner;
                if (!reply || !owner.starts_with(':')) { status_ = "unavailable"; (*callback)({}); return true; }
                auto message = bus_->createMethodCall(owner.c_str(), "/org/openkey/FocusedApplication",
                    "org.openkey.FocusedApplication1", "GetFocus");
                request_ = message.callAsync(150000, [this, generation, owner, callback](fcitx::dbus::Message& result) mutable {
                    if (generation != generation_) return true;
                    GnomeFocus focus;
                    focus.owner = owner;
                    if (result.type() != fcitx::dbus::MessageType::Reply || result.sender() != owner || result.signature() != "ssu") { status_ = "unavailable"; (*callback)({}); return true; }
                    result >> focus.app >> focus.session >> focus.serial;
                    bool sessionValid = focus.session.size() == 36;
                    for (std::size_t i = 0; sessionValid && i < focus.session.size(); ++i) {
                        const char c = focus.session[i];
                        sessionValid = (i == 8 || i == 13 || i == 18 || i == 23) ? c == '-' :
                            (c >= 'a' && c <= 'f') || (c >= '0' && c <= '9');
                    }
                    if (!result || !sessionValid || !focus.serial || (!focus.app.empty() && !GnomeFocus::validApp(focus.app))) {
                        status_ = "unavailable"; (*callback)({});
                    } else {
                        status_ = "connected";
                        if (focus.app.empty()) (*callback)({});
                        else (*callback)(std::move(focus));
                    }
                    return true;
                });
                if (!request_) { status_ = "unavailable"; (*callback)({}); }
                return true;
            });
    }
private:
    bool connect() {
        if (bus_ && bus_->isOpen()) return true;
        changed_.reset(); ownerChanged_.reset(); bus_.reset();
        try {
            bus_ = std::make_unique<fcitx::dbus::Bus>(fcitx::dbus::BusType::Session);
            if (!bus_->isOpen()) return false;
            bus_->attachEventLoop(&loop_);
            changed_ = bus_->addMatch(fcitx::dbus::MatchRule("org.gnome.Shell", "/org/openkey/FocusedApplication", "org.openkey.FocusedApplication1", "Changed"),
                [this](fcitx::dbus::Message&) { cancel(); status_ = "waiting"; invalidated_(); return true; });
            ownerChanged_ = bus_->addMatch(fcitx::dbus::MatchRule("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "NameOwnerChanged", {"org.gnome.Shell"}),
                [this](fcitx::dbus::Message&) { cancel(); status_ = "waiting"; invalidated_(); return true; });
            return changed_ && ownerChanged_;
        } catch (...) { changed_.reset(); ownerChanged_.reset(); bus_.reset(); return false; }
    }
    std::string status_ = "not-observed";
    fcitx::EventLoop& loop_;
    std::function<void()> invalidated_;
    uint64_t generation_ = 0;
    std::unique_ptr<fcitx::dbus::Bus> bus_;
    std::unique_ptr<fcitx::dbus::Slot> changed_, ownerChanged_, ownerRequest_, request_;
};
}
