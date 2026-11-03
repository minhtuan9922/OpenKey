#pragma once
#include <string>

namespace openkey {
// Status of Fcitx's most recent context, not a per-window or password-field probe.
enum class InputStatus { Telex, Vni, English, Inactive, Other, Unknown, Unavailable };
InputStatus classifyInput(int state, const std::string& method);
const char* statusName(InputStatus status);
InputStatus readFcitxStatus();
}
