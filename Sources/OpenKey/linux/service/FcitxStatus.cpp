#include "FcitxStatus.h"
#include "FcitxActions.h"
#include <systemd/sd-bus.h>
#include <memory>
#include <utility>

namespace openkey {
InputStatus classifyInput(int state, const std::string& method) {
    if ((state != 1 && state != 2) || method.empty()) return InputStatus::Unknown;
    if (method == "keyboard-us") return InputStatus::English;
    if (state == 1) return InputStatus::Inactive;
    if (method == "openkey-telex") return InputStatus::Telex;
    if (method == "openkey-vni") return InputStatus::Vni;
    return InputStatus::Other;
}
const char* statusName(InputStatus status) {
    switch (status) {
    case InputStatus::Telex: return "telex";
    case InputStatus::Vni: return "vni";
    case InputStatus::English: return "english";
    case InputStatus::Inactive: return "inactive";
    case InputStatus::Other: return "other";
    case InputStatus::Unavailable: return "unavailable";
    default: return "unknown";
    }
}
namespace {
using Bus = std::unique_ptr<sd_bus, decltype(&sd_bus_unref)>;
using Message = std::unique_ptr<sd_bus_message, decltype(&sd_bus_message_unref)>;
Message call(sd_bus* bus, const char* destination, const char* path,
             const char* interface, const char* method, const char* argument = nullptr) {
    sd_bus_message* raw = nullptr;
    if (sd_bus_message_new_method_call(bus, &raw, destination, path, interface, method) < 0)
        return Message(nullptr, sd_bus_message_unref);
    Message request(raw, sd_bus_message_unref);
    if (sd_bus_message_set_auto_start(request.get(), 0) < 0 ||
        (argument && sd_bus_message_append(request.get(), "s", argument) < 0))
        return Message(nullptr, sd_bus_message_unref);
    sd_bus_message* reply = nullptr;
    sd_bus_error error = SD_BUS_ERROR_NULL;
    const int result = sd_bus_call(bus, request.get(), 75000, &error, &reply);
    sd_bus_error_free(&error);
    if (result < 0) { sd_bus_message_unref(reply); reply = nullptr; }
    return Message(reply, sd_bus_message_unref);
}
bool state(sd_bus* bus, const char* owner, int& value) {
    auto reply = call(bus, owner, "/controller", "org.fcitx.Fcitx.Controller1", "State");
    return reply && sd_bus_message_has_signature(reply.get(), "i") > 0 &&
        sd_bus_message_read(reply.get(), "i", &value) > 0;
}
}
InputStatus readFcitxStatus() {
    sd_bus* raw = nullptr;
    const int connected = sd_bus_open_user(&raw);
    Bus bus(raw, sd_bus_unref);
    if (connected < 0) return InputStatus::Unavailable;
    // Do not auto-activate Fcitx. Pin all controller calls to this unique owner
    // so a daemon replacement cannot mix replies from two processes.
    auto ownerReply = call(bus.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "GetNameOwner", "org.fcitx.Fcitx5");
    const char* owner = nullptr;
    if (!ownerReply || sd_bus_message_has_signature(ownerReply.get(), "s") <= 0 ||
        sd_bus_message_read(ownerReply.get(), "s", &owner) <= 0 || !owner || owner[0] != ':')
        return InputStatus::Unavailable;
    int before = 0, after = 0;
    if (!state(bus.get(), owner, before)) return InputStatus::Unknown;
    auto methodReply = call(bus.get(), owner, "/controller", "org.fcitx.Fcitx.Controller1", "CurrentInputMethod");
    const char* method = nullptr;
    if (!methodReply || sd_bus_message_has_signature(methodReply.get(), "s") <= 0 ||
        sd_bus_message_read(methodReply.get(), "s", &method) <= 0 || !method ||
        !state(bus.get(), owner, after) || before != after) return InputStatus::Unknown;
    return classifyInput(after, method);
}
ActionResult switchFcitxInputMethod(InputMethod method) {
    const char* target = nullptr;
    if (method == InputMethod::Telex) target = "openkey-telex";
    else if (method == InputMethod::Vni) target = "openkey-vni";
    else if (method == InputMethod::English) target = "keyboard-us";
    else return ActionResult::Failed;
    sd_bus* raw = nullptr;
    const int connected = sd_bus_open_user(&raw);
    Bus bus(raw, sd_bus_unref);
    if (connected < 0) return ActionResult::Unavailable;
    auto ownerReply = call(bus.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "GetNameOwner", "org.fcitx.Fcitx5");
    const char* owner = nullptr;
    if (!ownerReply || sd_bus_message_read(ownerReply.get(), "s", &owner) <= 0 || !owner || owner[0] != ':')
        return ActionResult::Unavailable;
    constexpr const char* controller = "org.fcitx.Fcitx.Controller1";
    auto groupReply = call(bus.get(), owner, "/controller", controller, "CurrentInputMethodGroup");
    const char* group = nullptr;
    if (!groupReply || sd_bus_message_read(groupReply.get(), "s", &group) <= 0 || !group || !*group)
        return ActionResult::Failed;
    auto info = call(bus.get(), owner, "/controller", controller, "InputMethodGroupInfo", group);
    const char* layout = nullptr;
    if (!info || sd_bus_message_has_signature(info.get(), "sa(ss)") <= 0 ||
        sd_bus_message_read(info.get(), "s", &layout) <= 0 ||
        sd_bus_message_enter_container(info.get(), 'a', "(ss)") <= 0) return ActionResult::Failed;
    bool found = false;
    unsigned count = 0;
    int next = 0;
    while ((next = sd_bus_message_enter_container(info.get(), 'r', "ss")) > 0) {
        const char *name = nullptr, *itemLayout = nullptr;
        if (++count > 256 || sd_bus_message_read(info.get(), "ss", &name, &itemLayout) <= 0)
            return ActionResult::Failed;
        found |= std::string(name) == target;
        if (sd_bus_message_exit_container(info.get()) < 0) return ActionResult::Failed;
    }
    if (next < 0) return ActionResult::Failed;
    if (!found) return ActionResult::NotConfigured;
    // Best-effort group guard. Fcitx does not expose a compare-and-set operation.
    auto check = call(bus.get(), owner, "/controller", controller, "CurrentInputMethodGroup");
    const char* currentGroup = nullptr;
    if (!check || sd_bus_message_read(check.get(), "s", &currentGroup) <= 0) return ActionResult::Failed;
    if (std::string(currentGroup) != group) return ActionResult::GroupChanged;
    auto ownerCheck = call(bus.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "GetNameOwner", "org.fcitx.Fcitx5");
    const char* currentOwner = nullptr;
    if (!ownerCheck || sd_bus_message_read(ownerCheck.get(), "s", &currentOwner) <= 0 ||
        std::string(currentOwner) != owner) return ActionResult::Unavailable;
    auto changed = call(bus.get(), owner, "/controller", controller, "SetCurrentIM", target);
    if (!changed) return ActionResult::Unconfirmed; // Timeout may follow a successful write: never retry it.
    auto confirmed = call(bus.get(), owner, "/controller", controller, "CurrentInputMethod");
    const char* actual = nullptr;
    if (!confirmed || sd_bus_message_read(confirmed.get(), "s", &actual) <= 0 || std::string(actual) != target)
        return ActionResult::Unconfirmed;
    return ActionResult::Changed;
}
ActionResult openFcitxConfiguration() {
    sd_bus* raw = nullptr;
    const int connected = sd_bus_open_user(&raw);
    Bus bus(raw, sd_bus_unref);
    if (connected < 0) return ActionResult::Unavailable;
    auto ownerReply = call(bus.get(), "org.freedesktop.DBus", "/org/freedesktop/DBus",
                           "org.freedesktop.DBus", "GetNameOwner", "org.fcitx.Fcitx5");
    const char* owner = nullptr;
    if (!ownerReply || sd_bus_message_read(ownerReply.get(), "s", &owner) <= 0 || !owner || owner[0] != ':')
        return ActionResult::Unavailable;
    auto reply = call(bus.get(), owner, "/controller", "org.fcitx.Fcitx.Controller1", "Configure");
    // A controller acknowledgement is not proof that the settings window opened.
    return reply && sd_bus_message_has_signature(reply.get(), "") > 0 ?
        ActionResult::ConfigurationRequested : ActionResult::Unconfirmed;
}
const char* actionLabel(ActionResult result) {
    switch (result) {
    case ActionResult::Busy: return "Đang gửi yêu cầu đến Fcitx5…";
    case ActionResult::Changed: return "Đã xác nhận kiểu gõ từ Fcitx5";
    case ActionResult::ConfigurationRequested: return "Đã gửi yêu cầu mở cấu hình Fcitx5";
    case ActionResult::Unavailable: return "Fcitx5 chưa chạy; không tự khởi động";
    case ActionResult::NotConfigured: return "Thêm kiểu gõ vào nhóm Fcitx5 trước";
    case ActionResult::GroupChanged: return "Nhóm Fcitx5 đã đổi; vui lòng chọn lại";
    case ActionResult::Unconfirmed: return "Chưa xác nhận kết quả; kiểm tra Fcitx5 trước khi thử lại";
    case ActionResult::Failed: return "Không thực hiện được yêu cầu Fcitx5";
    default: return "Chọn kiểu gõ đã có trong nhóm Fcitx5";
    }
}
bool FcitxActions::request(InputMethod method) {
    return submit([method] { return switchFcitxInputMethod(method); });
}
bool FcitxActions::requestConfiguration() { return submit(openFcitxConfiguration); }
bool FcitxActions::submit(std::function<ActionResult()> task) {
    if (pending_.valid()) return false;
    try {
        pending_ = std::async(std::launch::async, std::move(task));
        result_ = ActionResult::Busy;
        return true;
    } catch (...) { result_ = ActionResult::Failed; return false; }
}
void FcitxActions::tick() {
    if (pending_.valid() && pending_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try { result_ = pending_.get(); } catch (...) { result_ = ActionResult::Failed; }
    }
}
}
