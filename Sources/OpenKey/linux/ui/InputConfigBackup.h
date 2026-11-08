#pragma once
#include <QString>
#include <QVariantMap>

namespace InputConfigBackup {
QString encode(const QVariantMap& runtime);
bool decode(const QString& json, QVariantMap& patch);
bool supported(const QVariantMap& patch, const QVariantMap& runtime);
}
