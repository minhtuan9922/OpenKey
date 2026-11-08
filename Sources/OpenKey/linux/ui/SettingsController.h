#pragma once
#include <QObject>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QVariantMap>
#include <functional>

// Fixed OpenKey config API. Never writes files or starts/restarts Fcitx.
class SettingsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString runtimeStyle READ runtimeStyle NOTIFY changed)
    Q_PROPERTY(bool commitOnlySupported READ commitOnlySupported NOTIFY changed)
    Q_PROPERTY(bool nativeDirectSupported READ nativeDirectSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeDiagnostics READ runtimeDiagnostics NOTIFY changed)
    Q_PROPERTY(bool spellingSupported READ spellingSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeCheckSpelling READ runtimeCheckSpelling NOTIFY changed)
    Q_PROPERTY(QString runtimeModernOrthography READ runtimeModernOrthography NOTIFY changed)
    Q_PROPERTY(QVariantMap runtimeAdvanced READ runtimeAdvanced NOTIFY changed)
    Q_PROPERTY(bool applicationRulesSupported READ applicationRulesSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeApplicationRules READ runtimeApplicationRules NOTIFY changed)
    Q_PROPERTY(bool macrosSupported READ macrosSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeMacrosEnabled READ runtimeMacrosEnabled NOTIFY changed)
    Q_PROPERTY(QString runtimeMacroTable READ runtimeMacroTable NOTIFY changed)
    Q_PROPERTY(bool quickTelexSupported READ quickTelexSupported NOTIFY changed)
    Q_PROPERTY(QString runtimeQuickTelex READ runtimeQuickTelex NOTIFY changed)
    Q_PROPERTY(QString result READ result NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
public:
    explicit SettingsController(const QDBusConnection& bus, QObject* parent = nullptr);
    QString runtimeStyle() const { return values_.value("InputStyle").toString(); }
    bool commitOnlySupported() const { return values_.value("CommitOnlySupported").toBool(); }
    bool nativeDirectSupported() const { return values_.value("NativeDirectSupported").toBool(); }
    QString runtimeDiagnostics() const { return values_.value("DirectDiagnostics").toString(); }
    bool spellingSupported() const { return values_.contains("CheckSpelling") && values_.contains("UseModernOrthography"); }
    QString runtimeCheckSpelling() const { return values_.value("CheckSpelling").toString(); }
    QString runtimeModernOrthography() const { return values_.value("UseModernOrthography").toString(); }
    QVariantMap runtimeAdvanced() const;
    bool applicationRulesSupported() const { return values_.contains("ApplicationRules"); }
    QString runtimeApplicationRules() const { return values_.value("ApplicationRules").toString(); }
    bool macrosSupported() const { return values_.contains("MacrosEnabled") && values_.contains("MacroTable"); }
    QString runtimeMacrosEnabled() const { return values_.value("MacrosEnabled").toString(); }
    QString runtimeMacroTable() const { return values_.value("MacroTable").toString(); }
    bool quickTelexSupported() const { return values_.contains("QuickTelex"); }
    QString runtimeQuickTelex() const { return values_.value("QuickTelex").toString(); }
    QString result() const { return result_; }
    bool busy() const { return busy_; }
    bool ready() const { return ready_; }
    Q_INVOKABLE void reload();
    Q_INVOKABLE void apply(const QString& style);
    Q_INVOKABLE void applyDiagnostics(const QString& value);
    Q_INVOKABLE void applySpelling(const QString& value);
    Q_INVOKABLE void applyModernOrthography(const QString& value);
    Q_INVOKABLE void applyAdvanced(const QString& key, const QString& value);
    Q_INVOKABLE void applyApplicationRules(const QString& value);
    Q_INVOKABLE void applyMacrosEnabled(const QString& value);
    Q_INVOKABLE void applyMacroTable(const QString& value);
    Q_INVOKABLE void applyQuickTelex(const QString& value);
    Q_INVOKABLE QString backupJson() const;
    Q_INVOKABLE QVariantMap inspectBackup(const QString& json);
    Q_INVOKABLE void restoreBackup(const QString& json);
signals:
    void changed();
private:
    using Callback = std::function<void(const QDBusMessage&)>;
    void call(const QString& destination, const QString& method, const QVariantList& arguments,
              Callback callback, bool busMethod = false);
    void resolve(Callback callback);
    void read(Callback callback);
    void finish(const QString& result, bool ready = false);
    void accept(const QString& result, const QVariantMap& values);
    void applyValue(const QString& key, const QString& value);
    static bool decode(const QDBusMessage& reply, QVariantMap& values);
    QDBusConnection bus_;
    QString owner_, result_ = "offline";
    QVariantMap values_;
    QString inspectedJson_;
    QVariantMap inspectedPatch_, inspectedBaseline_;
    bool busy_ = false, ready_ = false, writing_ = false;
    quint64 generation_ = 0;
};
