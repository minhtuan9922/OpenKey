#pragma once
#include <QString>
#include <QVariantMap>
#include "../MacroTable.h"

class ApplicationMemoryBackup {
public:
    static constexpr qsizetype MaxText = 65536;
    static QString encode(const QString& encoded);
    static bool decode(const QString& json, openkey::MacroTable::Entries& entries);
    static QVariantMap plan(const QString& json, const QString& mode, const QString& current);
};
