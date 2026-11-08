#pragma once
#include <QObject>
#include <QVariantList>

// Draft only. Applying to Fcitx requires an explicit SettingsController action.
class MacroModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList entries READ entries NOTIFY changed)
    Q_PROPERTY(QString encoded READ encoded NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
public:
    explicit MacroModel(QObject* parent = nullptr, bool applicationRules = false) : QObject(parent), applicationRules_(applicationRules) {}
    QVariantList entries() const { return entries_; }
    QString encoded() const;
    QString status() const { return status_; }
    Q_INVOKABLE bool load(const QString& encoded);
    Q_INVOKABLE bool put(const QString& key, const QString& value);
    Q_INVOKABLE bool remove(const QString& key);
    Q_INVOKABLE QString exportJson() const;
    Q_INVOKABLE bool importJson(const QString& json);
signals:
    void changed();
private:
    bool fail();
    bool applicationRules_;
    QVariantList entries_;
    QString status_ = "draft";
};
