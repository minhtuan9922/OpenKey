// Private D-Bus only: never acquires names or changes IM on the user's desktop bus.
#include "FcitxStatus.h"
#include "InputMonitor.h"
#include "FcitxActions.h"
#include <systemd/sd-bus.h>
#include <atomic>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Mock {
    sd_bus* bus = nullptr;
    sd_bus_slot* slot = nullptr;
    std::atomic<bool> stop{false}, fail{false}, failMethod{false}, changing{false}, slow{false};
    std::atomic<int> state{2}, reads{0}, methods{0};
    std::atomic<const char*> method{"openkey-telex"};
    std::atomic<bool> configured{true}, failWrite{false}, ignoreWrite{false}, changedGroup{false}, autoStart{false};
    std::atomic<bool> englishConfigured{true};
    std::atomic<bool> failConfiguration{false}, slowConfiguration{false};
    std::atomic<int> configurationRequests{0};
    std::atomic<int> writes{0}, groupReads{0};
    std::thread worker;
    ~Mock() {
        stop = true;
        if (worker.joinable()) worker.join();
        sd_bus_slot_unref(slot);
        sd_bus_unref(bus);
    }
    static int getState(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(userdata);
        const int read = ++self.reads;
        if (self.slow) std::this_thread::sleep_for(std::chrono::milliseconds(180));
        if (self.fail) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Test read failure");
        const int value = self.changing && read % 2 == 0 ? 1 : self.state.load();
        return sd_bus_reply_method_return(message, "i", value);
    }
    static int getMethod(sd_bus_message* message, void* userdata, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(userdata);
        ++self.methods;
        if (self.failMethod) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Test IM failure");
        return sd_bus_reply_method_return(message, "s", self.method.load());
    }
    static int getGroup(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(data);
        self.autoStart = self.autoStart || sd_bus_message_get_auto_start(message);
        const int read = ++self.groupReads;
        if (self.slow) std::this_thread::sleep_for(std::chrono::milliseconds(180));
        return sd_bus_reply_method_return(message, "s", self.changedGroup && read % 2 == 0 ? "Other" : "Default");
    }
    static int groupInfo(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(data);
        self.autoStart = self.autoStart || sd_bus_message_get_auto_start(message);
        const char* group = nullptr;
        if (sd_bus_message_read(message, "s", &group) <= 0 || std::string(group) != "Default")
            return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Wrong group");
        sd_bus_message* reply = nullptr;
        int result = sd_bus_message_new_method_return(message, &reply);
        if (result >= 0) result = sd_bus_message_append(reply, "s", "us");
        if (result >= 0) result = sd_bus_message_open_container(reply, 'a', "(ss)");
        if (self.configured) {
            if (result >= 0) result = sd_bus_message_append(reply, "(ss)", "openkey-telex", "");
            if (result >= 0) result = sd_bus_message_append(reply, "(ss)", "openkey-vni", "");
            if (result >= 0) result = sd_bus_message_append(reply, "(ss)", "openkey-simple-telex1", "");
            if (result >= 0) result = sd_bus_message_append(reply, "(ss)", "openkey-simple-telex2", "");
        }
        if (result >= 0 && self.englishConfigured)
            result = sd_bus_message_append(reply, "(ss)", "keyboard-us", "");
        if (result >= 0) result = sd_bus_message_close_container(reply);
        if (result >= 0) result = sd_bus_send(nullptr, reply, nullptr);
        sd_bus_message_unref(reply);
        return result < 0 ? result : 1;
    }
    static int setMethod(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(data);
        self.autoStart = self.autoStart || sd_bus_message_get_auto_start(message);
        const char* name = nullptr;
        if (sd_bus_message_read(message, "s", &name) <= 0) return -1;
        ++self.writes;
        if (self.failWrite) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Test write failure");
        if (!self.ignoreWrite) {
            if (std::string(name) == "openkey-telex") self.method = "openkey-telex";
            else if (std::string(name) == "openkey-vni") self.method = "openkey-vni";
            else if (std::string(name) == "openkey-simple-telex1") self.method = "openkey-simple-telex1";
            else if (std::string(name) == "openkey-simple-telex2") self.method = "openkey-simple-telex2";
            else if (std::string(name) == "keyboard-us") self.method = "keyboard-us";
            else return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_INVALID_ARGS, "Unsafe target");
        }
        return sd_bus_reply_method_return(message, "");
    }
    static int configure(sd_bus_message* message, void* data, sd_bus_error*) {
        auto& self = *static_cast<Mock*>(data);
        self.autoStart = self.autoStart || sd_bus_message_get_auto_start(message);
        ++self.configurationRequests;
        if (self.slowConfiguration) std::this_thread::sleep_for(std::chrono::milliseconds(180));
        if (self.failConfiguration) return sd_bus_reply_method_errorf(message, SD_BUS_ERROR_FAILED, "Test configure failure");
        return sd_bus_reply_method_return(message, "");
    }
    void start() {
        static const sd_bus_vtable vtable[] = {
            SD_BUS_VTABLE_START(0),
            SD_BUS_METHOD("State", "", "i", getState, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("CurrentInputMethod", "", "s", getMethod, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("CurrentInputMethodGroup", "", "s", getGroup, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("InputMethodGroupInfo", "s", "sa(ss)", groupInfo, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("SetCurrentIM", "s", "", setMethod, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_METHOD("Configure", "", "", configure, SD_BUS_VTABLE_UNPRIVILEGED),
            SD_BUS_VTABLE_END
        };
        require(sd_bus_open_user(&bus) >= 0, "Cannot connect mock to private bus");
        require(sd_bus_add_object_vtable(bus, &slot, "/controller", "org.fcitx.Fcitx.Controller1",
                                        vtable, this) >= 0, "Cannot register mock controller");
        require(sd_bus_request_name(bus, "org.fcitx.Fcitx5", 0) >= 0, "Cannot own private mock name");
        worker = std::thread([this] {
            while (!stop) {
                const int result = sd_bus_process(bus, nullptr);
                if (result < 0) { stop = true; return; }
                if (!result) sd_bus_wait(bus, 10000);
            }
        });
    }
};
}
int main() {
    using openkey::InputStatus;
    using openkey::ActionResult;
    using openkey::InputMethod;
    require(openkey::openFcitxConfiguration() == ActionResult::Unavailable,
            "Settings request must not auto-activate a missing daemon");
    require(openkey::switchFcitxInputMethod(InputMethod::Telex) == ActionResult::Unavailable,
            "Actions must not auto-activate a missing daemon");
    require(openkey::readFcitxStatus() == InputStatus::Unavailable, "Missing daemon must not be activated");
    require(openkey::classifyInput(2, "openkey-simple-telex1") == InputStatus::SimpleTelex1, "Simple Telex 1 classification");
    require(openkey::classifyInput(2, "openkey-simple-telex2") == InputStatus::SimpleTelex2, "Simple Telex 2 classification");
    require(openkey::classifyInput(0, "openkey-telex") == InputStatus::Unknown, "No recent context is unknown");
    require(openkey::classifyInput(7, "keyboard-us") == InputStatus::Unknown, "Invalid state must not show E");
    require(openkey::classifyInput(2, "") == InputStatus::Unknown, "Empty IM must not show V/E");
    Mock mock;
    mock.start();
    require(openkey::openFcitxConfiguration() == ActionResult::ConfigurationRequested &&
            mock.configurationRequests == 1 && mock.writes == 0,
            "Configure must only request the settings tool, not write input methods");
    mock.failConfiguration = true;
    require(openkey::openFcitxConfiguration() == ActionResult::Unconfirmed && mock.configurationRequests == 2,
            "Configure failure must not report an opened window or retry");
    mock.failConfiguration = false;
    mock.slowConfiguration = true;
    openkey::FcitxActions configurationActions;
    const auto configurationStarted = std::chrono::steady_clock::now();
    require(configurationActions.requestConfiguration() && !configurationActions.request(InputMethod::Telex) &&
            !configurationActions.requestConfiguration(), "Settings and IM requests must share one in-flight slot");
    configurationActions.tick();
    require(std::chrono::steady_clock::now() - configurationStarted < std::chrono::milliseconds(50),
            "Configure request must not block the event loop");
    while (configurationActions.result() == ActionResult::Busy) {
        require(std::chrono::steady_clock::now() - configurationStarted < std::chrono::seconds(1), "Configure timeout is unbounded");
        configurationActions.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(configurationActions.result() == ActionResult::Unconfirmed && mock.writes == 0,
            "Configure timeout must not claim success or change the input method");
    mock.slowConfiguration = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    require(mock.configurationRequests == 3 && !mock.autoStart, "Configure must not auto-retry or auto-start");
    require(openkey::switchFcitxInputMethod(static_cast<InputMethod>(99)) == ActionResult::Failed,
            "Only whitelisted input methods are allowed");
    require(openkey::switchFcitxInputMethod(InputMethod::English) == ActionResult::Changed &&
            std::string(mock.method.load()) == "keyboard-us", "English action must select and confirm US keyboard");
    mock.englishConfigured = false;
    const int beforeMissingEnglish = mock.writes;
    require(openkey::switchFcitxInputMethod(InputMethod::English) == ActionResult::NotConfigured &&
            mock.writes == beforeMissingEnglish, "Missing English keyboard must not mutate the group");
    mock.englishConfigured = true;
    require(openkey::switchFcitxInputMethod(InputMethod::Vni) == ActionResult::Changed &&
            std::string(mock.method.load()) == "openkey-vni", "VNI action must confirm the actual IM");
    require(openkey::switchFcitxInputMethod(InputMethod::Telex) == ActionResult::Changed,
            "Telex action must confirm the actual IM");
    require(openkey::switchFcitxInputMethod(InputMethod::SimpleTelex1) == ActionResult::Changed &&
            std::string(mock.method.load()) == "openkey-simple-telex1", "Simple Telex 1 action");
    require(openkey::switchFcitxInputMethod(InputMethod::SimpleTelex2) == ActionResult::Changed &&
            std::string(mock.method.load()) == "openkey-simple-telex2", "Simple Telex 2 action");
    require(openkey::switchFcitxInputMethod(InputMethod::Telex) == ActionResult::Changed, "Restore Telex fixture");
    const int writesBeforeGuards = mock.writes;
    mock.configured = false;
    require(openkey::switchFcitxInputMethod(InputMethod::Vni) == ActionResult::NotConfigured,
            "Unconfigured target must not be added or switched");
    mock.configured = true;
    mock.groupReads = 0;
    mock.changedGroup = true;
    require(openkey::switchFcitxInputMethod(InputMethod::Vni) == ActionResult::GroupChanged,
            "Changing group must cancel the action");
    require(mock.writes == writesBeforeGuards, "Preflight failures must not write");
    mock.changedGroup = false;
    mock.failWrite = true;
    const int beforeEnglishError = mock.writes;
    require(openkey::switchFcitxInputMethod(InputMethod::English) == ActionResult::Unconfirmed &&
            mock.writes == beforeEnglishError + 1, "English write error must not report success or auto-retry");
    require(openkey::switchFcitxInputMethod(InputMethod::Vni) == ActionResult::Unconfirmed,
            "Write failure must not report success");
    mock.failWrite = false;
    mock.ignoreWrite = true;
    require(openkey::switchFcitxInputMethod(InputMethod::Vni) == ActionResult::Unconfirmed,
            "Acknowledged but ineffective write must not report success");
    mock.ignoreWrite = false;
    mock.slow = true;
    openkey::FcitxActions actions;
    const auto actionStarted = std::chrono::steady_clock::now();
    require(actions.request(InputMethod::Vni) && !actions.request(InputMethod::Telex) && !actions.requestConfiguration(),
            "Only one command may be in flight");
    actions.tick();
    require(std::chrono::steady_clock::now() - actionStarted < std::chrono::milliseconds(50),
            "Menu request/tick must not wait for the controller");
    while (actions.result() == ActionResult::Busy) {
        require(std::chrono::steady_clock::now() - actionStarted < std::chrono::seconds(1), "Action timeout is unbounded");
        actions.tick();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    require(actions.result() == ActionResult::Failed && !mock.autoStart,
            "Slow preflight must fail without auto-activation");
    mock.slow = false;
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    require(openkey::readFcitxStatus() == InputStatus::Telex, "Active Telex must show Vietnamese");
    mock.method = "openkey-vni";
    require(openkey::readFcitxStatus() == InputStatus::Vni, "Active VNI must be distinct from Telex");
    mock.state = 1;
    require(openkey::readFcitxStatus() == InputStatus::Inactive, "Inactive saved OpenKey must not show V");
    mock.method = "keyboard-us";
    require(openkey::readFcitxStatus() == InputStatus::English, "US keyboard must show English");
    mock.state = 2;
    mock.method = "keyboard-fr";
    require(openkey::readFcitxStatus() == InputStatus::Other, "Other keyboard must not be mislabeled English");
    mock.method = "pinyin";
    require(openkey::readFcitxStatus() == InputStatus::Other, "Other IM must not show V/E");
    mock.state = 0;
    require(openkey::readFcitxStatus() == InputStatus::Unknown, "No context must be unknown on real IPC");
    mock.state = 2;
    mock.reads = 0;
    mock.changing = true;
    require(openkey::readFcitxStatus() == InputStatus::Unknown, "State changing during sample must be unknown");
    mock.changing = false;
    mock.fail = true;
    require(openkey::readFcitxStatus() == InputStatus::Unknown, "Controller error must not invent status");
    mock.fail = false;
    mock.failMethod = true;
    require(openkey::readFcitxStatus() == InputStatus::Unknown, "IM query failure must not invent status");
    mock.failMethod = false;
    mock.slow = true;
    const auto started = std::chrono::steady_clock::now();
    require(openkey::readFcitxStatus() == InputStatus::Unknown, "Slow controller must time out to unknown");
    require(std::chrono::steady_clock::now() - started < std::chrono::seconds(1), "Controller timeout is unbounded");
    mock.slow = false;
    // Let the deliberately late reply finish before the next isolated sample.
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    mock.method = "openkey-telex";
    require(openkey::readFcitxStatus() == InputStatus::Telex, "Reader must recover after error");
    require(mock.methods > 0 && mock.reads > 0, "Tests must use actual D-Bus calls");
    openkey::InputMonitor monitor(openkey::readFcitxStatus, std::chrono::milliseconds(10));
    const auto awaitStatus = [&](InputStatus expected) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (monitor.snapshot() != expected) {
            require(std::chrono::steady_clock::now() < deadline, "Private-bus monitor transition timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    };
    awaitStatus(InputStatus::Telex);
    mock.method = "openkey-vni";
    awaitStatus(InputStatus::Vni);
    mock.method = "keyboard-us";
    awaitStatus(InputStatus::English);
    mock.stop = true;
    mock.worker.join();
    require(sd_bus_release_name(mock.bus, "org.fcitx.Fcitx5") >= 0, "Cannot remove mock name");
    require(openkey::readFcitxStatus() == InputStatus::Unavailable, "Daemon disappearance must invalidate status");
    awaitStatus(InputStatus::Unavailable);
    // A new connection/unique owner takes over; no status from the old owner is cached.
    Mock replacement;
    replacement.method = "openkey-vni";
    replacement.start();
    require(openkey::readFcitxStatus() == InputStatus::Vni, "Daemon reappearance must recover to VNI");
    awaitStatus(InputStatus::Vni);
    std::cout << "Read-only Fcitx status: private-bus state mapping, errors and owner lifecycle passed\n";
}
