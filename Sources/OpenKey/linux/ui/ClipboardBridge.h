#pragma once
#include <QObject>
#include <QVariantMap>
#include <QMimeData>

// Non-owning test seam; production resolves QClipboard only on explicit calls.
class ClipboardAccess {
public:
    virtual ~ClipboardAccess() = default;
    virtual const QMimeData* mimeData() = 0;
    virtual void setText(const QString& text) = 0;
};

class ClipboardBridge : public QObject {
    Q_OBJECT
public:
    explicit ClipboardBridge(ClipboardAccess* access = nullptr, QObject* parent = nullptr);
    Q_INVOKABLE QVariantMap readText();
    Q_INVOKABLE QString copyText(const QString& text);
private:
    ClipboardAccess* access_;
};
