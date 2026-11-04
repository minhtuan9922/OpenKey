#include "ClipboardBridge.h"
#include "TextConverter.h"
#include <QClipboard>
#include <QGuiApplication>

namespace {
bool validText(const QString& text) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto unit = text.at(i);
        if (unit.isNull() || unit.isLowSurrogate()) return false;
        if (unit.isHighSurrogate() && (++i == text.size() || !text.at(i).isLowSurrogate())) return false;
    }
    return true;
}
QVariantMap failure(const QString& status) { return {{"ok", false}, {"status", status}}; }
QClipboard* systemClipboard() {
    return qobject_cast<QGuiApplication*>(QCoreApplication::instance()) ? QGuiApplication::clipboard() : nullptr;
}
}
ClipboardBridge::ClipboardBridge(ClipboardAccess* access, QObject* parent)
    : QObject(parent), access_(access) {}

QVariantMap ClipboardBridge::readText() {
    const QMimeData* mime = nullptr;
    if (access_) mime = access_->mimeData();
    else if (auto* clipboard = systemClipboard()) mime = clipboard->mimeData(QClipboard::Clipboard);
    if (!mime) return failure("unavailable");
    if (!mime->hasText()) return failure("not-text");
    const auto text = mime->text();
    if (text.isEmpty()) return failure("empty");
    if (text.size() > TextConverter::inputLimit) return failure("too-large");
    if (!validText(text)) return failure("invalid");
    return {{"ok", true}, {"status", "pasted"}, {"text", text}};
}
QString ClipboardBridge::copyText(const QString& text) {
    // Validate before touching the clipboard, never clear it on failure.
    if (text.isEmpty()) return QStringLiteral("empty");
    if (text.size() > TextConverter::outputLimit) return QStringLiteral("too-large");
    if (!validText(text)) return QStringLiteral("invalid");
    if (access_) access_->setText(text);
    else if (auto* clipboard = systemClipboard()) clipboard->setText(text, QClipboard::Clipboard);
    else return QStringLiteral("unavailable");
    return QStringLiteral("copied");
}
