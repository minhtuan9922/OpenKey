#pragma once

#include "ConvertService.h"
#include "../../../engine/ConvertTool.h"

// Engine-only bridge: the Fluent service must not depend on legacy globals.
namespace LegacyQuickConvert {
inline ConvertService::ClipboardResult convertClipboard(HWND owner) {
    ConvertService::Options options;
    options.fromCode = convertToolFromCode;
    options.toCode = convertToolToCode;
    options.toAllCaps = convertToolToAllCaps;
    options.toAllNonCaps = convertToolToAllNonCaps;
    options.toCapsFirstLetter = convertToolToCapsFirstLetter;
    options.toCapsEachWord = convertToolToCapsEachWord;
    options.removeMark = convertToolRemoveMark;
    return ConvertService::convertClipboard(options, owner);
}
}
