#include "SettingsActivation.h"
#include <QDBusMessage>

namespace {
const QString service = QStringLiteral("org.openkey.Settings");
const QString path = QStringLiteral("/org/openkey/Settings");
}
SettingsActivation::~SettingsActivation() {
    if (owns_) {
        bus_.unregisterService(service);
        bus_.unregisterObject(path);
    }
}
const QStringList& SettingsActivation::pages() {
    static const QStringList values{"overview", "spelling", "hotkeys", "compatibility",
                                    "advanced", "macro", "convert", "about"};
    return values;
}
void SettingsActivation::setWindow(QQuickWindow* window) {
    window_ = window;
    if (window_ && requestedPage_ >= 0) window_->setProperty("pageIndex", requestedPage_);
}
bool SettingsActivation::ShowPage(const QString& page) {
    const auto index = pages().indexOf(page);
    if (index < 0) return false;
    requestedPage_ = index;
    if (window_) window_->setProperty("pageIndex", requestedPage_);
    Show();
    return true;
}
SettingsActivation::Claim SettingsActivation::claim() {
    if (owns_ || !bus_.isConnected()) return Claim::Failed;
    // Object first; do not advertise a name without its handler. Never replace
    // another process or remove an object registered by another instance.
    if (!bus_.registerObject(path, this, QDBusConnection::ExportScriptableSlots)) return Claim::Failed;
    if (bus_.registerService(service)) {
        owns_ = true;
        return Claim::Primary;
    }
    bus_.unregisterObject(path);
    auto ownerRequest = QDBusMessage::createMethodCall("org.freedesktop.DBus", "/org/freedesktop/DBus",
        "org.freedesktop.DBus", "GetNameOwner");
    ownerRequest.setAutoStartService(false);
    ownerRequest << service;
    const auto ownerReply = bus_.call(ownerRequest, QDBus::Block, 750);
    if (ownerReply.type() != QDBusMessage::ReplyMessage || ownerReply.signature() != "s" ||
        ownerReply.arguments().size() != 1) return Claim::Failed;
    const auto owner = ownerReply.arguments().first().toString();
    if (!owner.startsWith(':')) return Claim::Failed;
    auto request = QDBusMessage::createMethodCall(owner, path, "org.openkey.Settings1", "Show");
    request.setAutoStartService(false);
    const auto reply = bus_.call(request, QDBus::Block, 1000);
    return reply.type() == QDBusMessage::ReplyMessage && reply.arguments().isEmpty() ?
        Claim::Forwarded : Claim::Failed;
}
void SettingsActivation::Show() {
    if (window_) {
        if (window_->visibility() == QWindow::Minimized) window_->showNormal();
        else window_->show();
        window_->raise();
        window_->requestActivate(); // Wayland compositor may refuse focus stealing.
    }
    emit showRequested();
}
