#include "Composition.h"
#include "DirectInput.h"
#include "../MacroTable.h"
#include "../ApplicationRules.h"
#include "../ApplicationMemory.h"
#include "GnomeFocusBridge.h"
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <fcitx-config/enum.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/key.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

namespace {
FCITX_CONFIG_ENUM(InputStyle, Preedit, Direct, CommitOnly, NativeDirect);
FCITX_CONFIGURATION(InputConfig,
    fcitx::Option<InputStyle> style{this, "InputStyle", "Chế độ gõ / Input style", InputStyle::Preedit};
    fcitx::Option<bool> terminalDeleteFix{this, "TerminalDeleteFix", "Sửa dấu trong terminal VS Code/Cursor bằng DEL (thử nghiệm)", false};
    fcitx::Option<bool> diagnostics{this, "DirectDiagnostics", "Direct diagnostics (no text content)", false};
    fcitx::Option<bool> rememberApplication{this, "RememberInputMethodPerApplication", "Ghi nhớ bộ gõ theo ứng dụng", false};
    fcitx::Option<bool> gnomeIdentity{this, "UseGnomeApplicationIdentity", "Nhận diện ứng dụng qua extension GNOME", false};
    fcitx::Option<std::string> applicationRules{this, "ApplicationRules", "Chế độ theo ứng dụng / Application rules (encoded)", ""};
    fcitx::Option<bool> macrosEnabled{this, "MacrosEnabled", "Bật gõ tắt / Enable macros", false};
    fcitx::Option<std::string> macroTable{this, "MacroTable", "Bảng gõ tắt / Macro table (encoded)", ""};
    fcitx::Option<bool> freeMark{this, "FreeMark", "Đặt dấu tự do / Free tone placement", false};
    fcitx::Option<bool> allowConsonantZFWJ{this, "AllowConsonantZFWJ", "Cho phép phụ âm Z F W J", false};
    fcitx::Option<bool> quickStartConsonant{this, "QuickStartConsonant", "Gõ nhanh phụ âm đầu", false};
    fcitx::Option<bool> quickEndConsonant{this, "QuickEndConsonant", "Gõ nhanh phụ âm cuối", false};
    fcitx::Option<bool> restoreWrongSpelling{this, "RestoreWrongSpelling", "Khôi phục từ sai chính tả", false};
    fcitx::Option<bool> quickTelex{this, "QuickTelex", "Gõ nhanh Telex / Quick Telex", false};
    fcitx::Option<bool> checkSpelling{this, "CheckSpelling", "Kiểm tra chính tả / Check spelling", true};
    fcitx::Option<bool> useModernOrthography{this, "UseModernOrthography", "Đặt dấu kiểu mới / Modern tone placement", true};
);
FCITX_CONFIGURATION(ApplicationMemoryConfig,
    fcitx::Option<std::string> entries{this, "Entries", "Remembered input methods", ""};
);
// Runtime-only management endpoint; never merged into the main input config.
FCITX_CONFIGURATION(ApplicationMemoryView,
    fcitx::Option<std::string> entries{this, "Entries", "Remembered input methods", ""};
    fcitx::Option<std::string> expected{this, "ExpectedEntries", "Compare before write", ""};
    fcitx::Option<std::string> application{this, "CurrentApplication", "Identified focused application", ""};
    fcitx::Option<std::string> identity{this, "IdentityStatus", "GNOME identity connection", "disabled"};
);
struct QueuedKey { fcitx::Key key; bool release; int time; };
bool printable(uint32_t value) {
    return (value >= 32 && value < 127) ||
        (value >= 160 && fcitx::utf8::UCS4IsValid(value));
}
bool letterOrDigit(uint32_t symbol) {
    return (symbol >= 'a' && symbol <= 'z') ||
        (symbol >= 'A' && symbol <= 'Z') || (symbol >= '0' && symbol <= '9');
}
constexpr fcitx::KeyStates kShortcutMods{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
    fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
bool shortcutKey(const fcitx::Key& key) {
    return static_cast<bool>(key.states() & kShortcutMods);
}
// Letters/digits/printable delimiters/Backspace use commitString/deleteSurroundingText.
// Enter/navigation/shortcuts MUST stay unaccepted so GNOME Wayland delivers them
// natively. alpha.13's filterAndAccept+forwardKey swallowed keys (forwardKey is
// not reliable on this stack). End Direct ownership, then pass the key through.
bool composingKey(const fcitx::Key& key, uint32_t symbol) {
    if (shortcutKey(key) || key.isModifier()) return false;
    if (letterOrDigit(symbol)) return true;
    if (printable(symbol)) return true;
    return key.sym() == FcitxKey_BackSpace;
}
bool enterKey(const fcitx::Key& key) {
    return key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter;
}
bool navigationKey(const fcitx::Key& key) {
    const auto sym = key.sym();
    if (sym == FcitxKey_Tab || sym == FcitxKey_ISO_Left_Tab || sym == FcitxKey_Escape)
        return true;
    if (sym >= FcitxKey_Home && sym <= FcitxKey_Insert) return true;
    if (sym >= FcitxKey_KP_Home && sym <= FcitxKey_KP_Delete) return true;
    return false;
}
enum class DirectOutcome { Ignored, Consumed };

// LibreOffice advertises SurroundingText on Wayland but often drops the first
// character (tieengs → iếng). Keep Direct Enter/nav unaccepted; for these apps
// prefer Preedit so characters are painted before commit.
// GNOME's ibus bridge usually reports program=gnome-shell, so this only catches
// frontends that still pass the real process name. The native-prime timeout
// covers the rest.
bool surroundingTextUnreliable(fcitx::InputContext* ic) {
    auto program = ic->program();
    if (program.empty()) return false;
    const auto slash = program.find_last_of('/');
    if (slash != std::string::npos) program = program.substr(slash + 1);
    for (char& ch : program)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    return program == "soffice.bin" || program == "soffice" || program == "swriter" ||
        program == "scalc" || program == "simpress" ||
        program.find("libreoffice") != std::string::npos;
}

// GNOME's ibus bridge is the Cursor chat and editor frontend. Direct mode must
// not send a client preedit, or the word is underlined. The composed syllable
// is written into the document on each key so the letters are visible at once.
bool ibusClient(fcitx::InputContext* ic) {
    const char* name = ic->frontend();
    return name && name[0] == 'i' && name[1] == 'b' && name[2] == 'u' &&
        name[3] == 's' && name[4] == '\0';
}

// Chrome fields advertise spellcheck, URL or email hints and
// ignore deleteSurroundingText. Replacing o with ô then leaves goô. Cursor's
// chat does not advertise those hints, and deleting an ASCII vowel there works.
bool asciiDeleteUnreliable(fcitx::InputContext* ic) {
    const auto flags = ic->capabilityFlags();
    // Not UppwercaseSentences alone: Cursor chat reports it (caps 1048658)
    // and honours deletes; a loose combining mark there showed mẻ / mr.
    // Only address-bar style fields (URL/email). Spellcheck/sentence fields
    // (Cursor chat, ordinary textareas) honour deletes and must stay NFC.
    return static_cast<bool>(flags & fcitx::CapabilityFlag::Url) ||
        static_cast<bool>(flags & fcitx::CapabilityFlag::Email);
}

// Terminals cannot take a Direct rewrite or a client preedit: xterm.js (VS
// Code/Cursor terminal) sends only appended text to the PTY, so noo -> noô,
// laf -> laà, dd -> dđ. Only the explicit Terminal flag selects commit-only
// mode. alpha.28/.29 also keyed on WordCompletion / missing SurroundingText;
// GNOME reports SurroundingText late on focus in ordinary fields, so the first
// key went to preedit and broke the word (mer -> mr, mẻ with a loose mark).
// Cursor's terminal reports the same flags as its editor (82), so it is not
// detected here and stays a documented FAIL.
bool appendOnlyClient(fcitx::InputContext* ic) {
    return static_cast<bool>(ic->capabilityFlags() & fcitx::CapabilityFlag::Terminal);
}
bool directRewriteUnsafe(fcitx::InputContext* ic) { return appendOnlyClient(ic); }

// True when the unaccepted first Direct key actually landed as exactly one
// codepoint in front of the caret. Only then may a later Preedit commit delete
// it; anything else is left untouched.
bool nativePrimeLanded(const openkey::Snapshot& base, char32_t primed, const openkey::Snapshot& snap) {
    if (!primed || snap.cursor != snap.anchor || base.cursor != base.anchor) return false;
    if (snap.cursor != base.cursor + 1) return false;
    openkey::Snapshot expected = base;
    expected.text.insert(expected.cursor, 1, primed);
    expected.cursor += 1;
    expected.anchor = expected.cursor;
    return snap == expected;
}

// A notification that still lies on the way from the last confirmed baseline to
// the prediction (the first keys of a fast word). Not a different field.
bool nativeStillTyping(const openkey::Snapshot& observed, const openkey::Snapshot& expected,
                       const openkey::Snapshot& snap) {
    if (snap.cursor != snap.anchor || observed.cursor != observed.anchor || expected.cursor != expected.anchor)
        return false;
    if (snap.text.size() <= observed.text.size() || snap.text.size() >= expected.text.size()) return false;
    if (snap.text.compare(0, observed.text.size(), observed.text) != 0) return false;
    if (expected.text.compare(0, snap.text.size(), snap.text) != 0) return false;
    return snap.cursor == snap.text.size() && snap.cursor <= expected.cursor;
}

// A shorter caret-at-end snapshot that still starts like this word is our own
// delayed echo (delete then commit). A longer buffer, or a caret that is not
// at its end, is surrounding text for some other span.
bool nativeOwnEcho(const openkey::Snapshot& expected, const openkey::Snapshot& snap) {
    if (snap.cursor != snap.anchor || snap.cursor != snap.text.size()) return false;
    if (snap.text.size() > expected.text.size()) return false;
    // An empty cache is a focus change, not an echo of this word.
    if (snap.text.empty() || expected.text.empty()) return false;
    return snap.text.front() == expected.text.front();
}

// The caret sits on a copy of this syllable in some other span. Deleting there
// would edit the wrong context.
bool nativeSuffixElsewhere(const std::u32string& placed, const openkey::Snapshot& snap) {
    if (placed.empty() || snap.cursor != snap.anchor || snap.cursor < placed.size()) return false;
    return snap.text.compare(snap.cursor - placed.size(), placed.size(), placed) == 0;
}

// The notification is the caret buffer we predicted, possibly with a longer
// prefix. A stale prediction (built from some other span) is not a suffix.
bool nativeExpectedSuffix(const openkey::Snapshot& expected, const openkey::Snapshot& snap) {
    if (expected.cursor != expected.anchor || expected.cursor != expected.text.size()) return false;
    if (snap.cursor != snap.anchor || snap.cursor != snap.text.size()) return false;
    if (snap.text.size() < expected.text.size()) return false;
    return snap.text.compare(snap.text.size() - expected.text.size(), expected.text.size(),
                             expected.text) == 0;
}

void popCharacter(std::string& text) {
    auto value = openkey::codepoints(text);
    if (!value.empty()) value.pop_back();
    text = openkey::utf8(value);
}
struct State : fcitx::InputContextProperty {
    openkey::Composition composition;
    std::optional<openkey::Composition> previousWord;
    std::string trailingSpaces;
    openkey::DirectInput tracker;
    bool direct = false;
    bool bypass = false;
    bool emitting = false;
    bool active = false;
    bool pendingNotified = false;
    bool primed = false; // first Direct character already handed to the client
    bool nativePrime = false; // that character was an unaccepted native key, still unACKed
    bool owned = false; // word is in the document via commitString, not surrounding ACK
    bool retainOwned = false; // Space ended the word; Backspace may reopen it
    unsigned ownedLen = 0;
    unsigned ownedSkip = 0; // native first character already in the document
    bool forcePreedit = false; // this context must not return to Direct
    bool hiddenPreedit = false; // append-only client: never send a client preedit
    bool plainCommit = false; // ibus Direct word is already in the document; do not underline or insert it again
    bool ibusAfterEnter = false;
    bool ibusNativeWord = false; // unconfirmed native prime: pass the remaining word through
    std::optional<openkey::Snapshot> ibusPrimeExpected;
    std::optional<openkey::Snapshot> nativeExpected;
    std::optional<openkey::Snapshot> nativeObserved; // last notified baseline; cache may stay here while we type
    std::optional<openkey::Snapshot> nativeCompletion;
    bool nativeRawWord = false;
    bool nativeKeepOnReset = false; // editor reset after our own commit is not a new word
    bool nativeResetHeld = false; // that syllable must be dropped if the caret then moves
    bool nativeAwaitingUpdate = false;
    // URL completion clear in flight: the new tail was committed over the
    // selected suggestion (go|ogle.com -> goô). The caret edit finishing the
    // word runs only after the client reports exactly `stage` (that text with
    // a collapsed caret at its end).
    struct NativeClear { openkey::Snapshot base, stage; std::u32string after; unsigned removed; };
    std::optional<NativeClear> nativeClear;
    // This field autocompleted a committed tail too: stop two-step clears
    // until focus changes, so intermediate tails never pile up.
    bool nativeClearBlocked = false;
    // Chrome sends no surrounding text after Ctrl+L, Ctrl+A, Delete or
    // Backspace (ibus bus capture, 2026-11-09 14:48): the cache still shows the
    // text from before. Set when such a key passes natively; any update clears it.
    bool nativeStale = false;
    // First letter typed over a stale cache: adopt the next update as the
    // baseline if the character before its caret is this letter.
    char32_t nativeAdopt = 0;
    // Diagnostics only: what the client reports around a URL completion clear.
    struct ClearTrace { openkey::Snapshot base; uint64_t started = 0; unsigned events = 0, left = 0; };
    std::optional<ClearTrace> clearTrace;
    std::string terminalWord; // Only text committed by the opt-in terminal path, never native keys.
    std::string placed; // UTF-8 of that word currently in the document
    bool preeditDrifted = false; // surrounding changed; keep the syllable until Enter leaves it
    bool repairNativePrime = false;
    char32_t nativePrimeChar = 0;
    std::optional<openkey::Snapshot> nativePrimeBase;
    std::string recovery;
    std::string lastReason;
    std::deque<QueuedKey> queue;
    std::unique_ptr<fcitx::EventSourceTime> timeout;
};

bool nativeResetStillHere(const State& state, const std::optional<openkey::Snapshot>& snapshot) {
    if (!snapshot || !state.nativeExpected) return false;
    if (*snapshot == *state.nativeExpected) return true;
    if (nativeOwnEcho(*state.nativeExpected, *snapshot)) return true;
    return nativeSuffixElsewhere(openkey::codepoints(state.placed), *snapshot);
}

class OpenKeyAddon : public fcitx::InputMethodEngine {
public:
    explicit OpenKeyAddon(fcitx::Instance* instance) : instance_(instance),
        gnomeBridge_(instance->eventLoop(), [this] { bridgeFocus_.reset(); ++memoryEpoch_; }) {
        instance->inputContextManager().registerProperty("openkeyState", &factory_);
        fcitx::readAsIni(config_, "conf/openkey.conf");
        openkey::ApplicationRules::parse(*config_.applicationRules, appRules_);
        loadApplicationMemory();
        memoryWatchers_.push_back(instance->watchEvent(fcitx::EventType::InputContextFocusIn,
            fcitx::EventWatcherPhase::PostInputMethod, [this](fcitx::Event& event) {
                auto* ic = static_cast<fcitx::InputContextEvent&>(event).inputContext();
                // Core focus activation has completed. Its incidental switch
                // notifications must not replace this application's preference.
                resetMemoryFocus();
                if (baseMemoryEligible(ic) && gnomeContext(ic) && *config_.gnomeIdentity) {
                    memoryFocus_ = ic;
                    const auto epoch = memoryEpoch_, keys = memoryKeys_;
                    resolveGnome([this, ic, epoch, keys](const auto& focus) {
                        if (epoch != memoryEpoch_ || memoryFocus_ != ic || !baseMemoryEligible(ic) || !focus) return;
                        bridgeFocus_ = focus;
                        // Never change method after typing began while waiting
                        // for the bridge, or while a composition is in flight.
                        const auto& state = *ic->propertyFor(&factory_);
                        if (keys == memoryKeys_ && state.composition.empty() && state.queue.empty() && state.recovery.empty())
                            restoreApplication(ic);
                    });
                } else if (memoryEligible(ic)) {
                    memoryFocus_ = ic;
                    restoreApplication(ic);
                }
            }));
        for (auto type : {fcitx::EventType::InputContextFocusOut, fcitx::EventType::InputContextDestroyed}) {
            memoryWatchers_.push_back(instance->watchEvent(type, fcitx::EventWatcherPhase::PreInputMethod,
                [this](fcitx::Event& event) {
                    if (static_cast<fcitx::InputContextEvent&>(event).inputContext() == memoryFocus_)
                        resetMemoryFocus();
                }));
        }
        memoryWatchers_.push_back(instance->watchEvent(fcitx::EventType::InputContextSwitchInputMethod,
            fcitx::EventWatcherPhase::PostInputMethod, [this](fcitx::Event& event) {
                auto& change = static_cast<fcitx::InputContextSwitchInputMethodEvent&>(event);
                if (change.inputContext() != memoryFocus_ || restoringMemory_ ||
                    change.reason() == fcitx::InputMethodSwitchedReason::CapabilityChanged ||
                    change.reason() == fcitx::InputMethodSwitchedReason::GroupChange) return;
                auto* ic = change.inputContext();
                if (gnomeContext(ic) && *config_.gnomeIdentity && baseMemoryEligible(ic)) {
                    const auto epoch = memoryEpoch_;
                    const auto method = instance_->inputMethod(ic);
                    resolveGnome([this, ic, epoch, method](const auto& focus) {
                        if (epoch != memoryEpoch_ || memoryFocus_ != ic || !baseMemoryEligible(ic) || !focus ||
                            instance_->inputMethod(ic) != method) return;
                        bridgeFocus_ = focus;
                        rememberApplication(ic);
                    });
                } else rememberApplication(ic);
            }));
        memoryWatchers_.push_back(instance->watchEvent(fcitx::EventType::InputContextKeyEvent,
            fcitx::EventWatcherPhase::PreInputMethod, [this](fcitx::Event& event) {
                auto& key = static_cast<fcitx::KeyEvent&>(event);
                if (key.inputContext() == memoryFocus_ && !key.isRelease() && !key.key().isModifier()) ++memoryKeys_;
            }));
        surroundingWatcher_ = instance->watchEvent(fcitx::EventType::InputContextSurroundingTextUpdated,
            fcitx::EventWatcherPhase::InputMethod, [this](fcitx::Event& event) {
                auto* ic = static_cast<fcitx::InputContextEvent&>(event).inputContext();
                auto& state = *ic->propertyFor(&factory_);
                if (!state.active) return;
                traceClear(ic, "update");
                if (styleFor(ic) == InputStyle::NativeDirect &&
                    (state.nativeExpected || state.nativeObserved)) {
                    // deleteSurroundingText notifies before commitString. That
                    // intermediate snapshot is not a focus change.
                    if (state.emitting) return;
                    const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
                    state.nativeStale = false;
                    if (state.nativeAdopt) {
                        const auto letter = std::exchange(state.nativeAdopt, U'\0');
                        if (adoptNativeBase(ic, snapshot, letter)) return;
                    }
                    if (state.nativeClear) {
                        // Only the exact committed tail with a collapsed caret
                        // authorizes the edit; anything else waits for the next key.
                        resolveNativeClear(ic, false);
                        return;
                    }
                    // A text editor wipes surrounding text when it resets after
                    // commitString. That empty cache is not a focus change; hôị
                    // and côngj were the tone key starting a new syllable.
                    if (snapshot && snapshot->text.empty() && snapshot->cursor == snapshot->anchor &&
                        !state.composition.empty() && !state.placed.empty() &&
                        (state.nativeKeepOnReset || state.nativeResetHeld))
                        return;
                    if (state.nativeResetHeld && !nativeResetStillHere(state, snapshot)) {
                        state.composition.clear();
                        state.placed.clear();
                        state.nativeExpected.reset();
                        state.nativeObserved.reset();
                        state.nativeCompletion.reset();
                        state.nativeAwaitingUpdate = false;
                        state.nativeRawWord = false;
                        state.nativeResetHeld = false;
                        return;
                    }
                    if (state.nativeResetHeld) state.nativeResetHeld = false;
                    state.nativeCompletion.reset();
                    if (snapshot && state.nativeExpected && *snapshot == *state.nativeExpected) {
                        state.nativeAwaitingUpdate = false;
                        state.nativeObserved = snapshot;
                    } else if (snapshot && state.nativeExpected &&
                               (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
                               openkey::selectedUrlCompletion(*state.nativeExpected, *snapshot)) {
                        // This notification confirms every typed character and
                        // its caret. Keep the actual selected suffix separately;
                        // the prediction is NOT installed in the client's cache.
                        state.nativeCompletion = snapshot;
                        state.nativeAwaitingUpdate = false;
                    } else if (snapshot && state.nativeObserved && state.nativeExpected &&
                               nativeStillTyping(*state.nativeObserved, *state.nativeExpected, *snapshot)) {
                        state.nativeObserved = snapshot;
                    } else if (snapshot && snapshot->cursor != snapshot->anchor) {
                        // A selection is not a focus change. Keep the prediction
                        // so the next key can report the mismatch and stop.
                    } else if (!(snapshot && state.nativeObserved && *snapshot == *state.nativeObserved)) {
                        // An empty snapshot is a focus change. Cursor chat also
                        // sends surrounding text that is not the caret word;
                        // dropping the syllable there left tieengs raw.
                        const bool typing = !state.composition.empty() && !state.placed.empty();
                        const bool ours = snapshot && state.nativeExpected &&
                            nativeOwnEcho(*state.nativeExpected, *snapshot);
                        const bool elsewhere = snapshot &&
                            nativeSuffixElsewhere(openkey::codepoints(state.placed), *snapshot);
                        const bool foreign = typing && snapshot && !snapshot->text.empty() && !ours &&
                            !elsewhere && !(ic->capabilityFlags() & fcitx::CapabilityFlag::Url);
                        // The caret document ends with the letters we just placed, but
                        // the prediction was built from a different span (Cursor chat
                        // often reports the wrong surrounding text). Rebase onto the
                        // caret document. A longer buffer that already ends with the
                        // prediction is a different context and must still be dropped.
                        const bool caughtUp = typing && elsewhere && state.nativeExpected && snapshot &&
                            !nativeExpectedSuffix(*state.nativeExpected, *snapshot);
                        if (caughtUp) {
                            state.nativeExpected = snapshot;
                            state.nativeObserved = snapshot;
                            state.nativeAwaitingUpdate = false;
                            state.nativeCompletion.reset();
                        } else if (typing && (ours || foreign || state.nativeKeepOnReset || state.nativeResetHeld)) {
                            // A shorter echo of this edit, or the editor reset
                            // after ô, is still this syllable. Dropping it made
                            // j tone only the following i (hôị) or stay literal (côngj).
                        } else if (!foreign) {
                            state.composition.clear();
                            state.placed.clear();
                            state.nativeExpected.reset();
                            state.nativeObserved.reset();
                            state.nativeCompletion.reset();
                            state.nativeAwaitingUpdate = false;
                            state.nativeRawWord = false;
                        }
                    }
                    return;
                }
                if (state.direct && state.tracker.pending()) state.pendingNotified = true;
                if (state.emitting) return;
                if (state.forcePreedit && !state.owned && !state.retainOwned) {
                    const auto snap = openkey::Snapshot::read(ic->surroundingText(), true);
                    const bool stale = snap && state.nativePrimeBase && *snap == *state.nativePrimeBase;
                    const bool landed = snap && state.nativePrimeBase && state.nativePrimeChar &&
                        nativePrimeLanded(*state.nativePrimeBase, state.nativePrimeChar, *snap);
                    const bool selection = snap && snap->cursor != snap->anchor;
                    // Cursor chat reports surrounding text that is not the caret
                    // word. Dropping the syllable here turned tieengs into eengs.
                    // Keep it. Enter/Escape still abandons it if the user left.
                    if (stale || landed) state.preeditDrifted = false;
                    else if (!selection) state.preeditDrifted = true;
                    return;
                }
                if (state.owned || state.retainOwned) {
                    // The word is already in the document. A late ACK must not
                    // drop it. A new selection means the caret left the word.
                    const auto snap = openkey::Snapshot::read(ic->surroundingText(), true);
                    if (snap && snap->cursor != snap->anchor) {
                        state.owned = false;
                        state.retainOwned = false;
                        state.ownedLen = 0;
                        state.ownedSkip = 0;
                        state.plainCommit = false;
                        state.placed.clear();
                        state.direct = false;
                        state.composition.clear();
                        state.previousWord.reset();
                        state.trailingSpaces.clear();
                    }
                    return;
                }
                if (state.direct) reconcile(ic, true);
                else if (state.bypass && state.recovery.empty() && state.queue.empty() &&
                         openkey::Snapshot::read(ic->surroundingText(), true)) {
                    // A reused browser context must not require an EN-VI
                    // toggle. New input will own only a fresh suffix.
                    clearDirect(state);
                    state.composition.clear();
                    diagnose(ic, "context-ready");
                    update(ic);
                }
            });
    }

    const fcitx::Configuration* getConfig() const override { return &config_; }
    const fcitx::Configuration* getSubConfig(const std::string& path) const override {
        if (path != "application-memory") return nullptr;
        memoryView_.entries.setValue(applicationMemory_.encode());
        memoryView_.expected.setValue("");
        memoryView_.application.setValue(memoryEligible(memoryFocus_) ? memoryProgram(memoryFocus_) : "");
        memoryView_.identity.setValue(!*config_.rememberApplication || !*config_.gnomeIdentity ?
            "disabled" : gnomeBridge_.status());
        return &memoryView_;
    }
    void setSubConfig(const std::string& path, const fcitx::RawConfig& raw) override {
        if (path != "application-memory") return;
        const auto* expected = raw.valueByPath("ExpectedEntries");
        const auto* entries = raw.valueByPath("Entries");
        bool valid = true;
        raw.visitSubItems([&valid](const fcitx::RawConfig&, const std::string& key) {
            if (key != "Entries" && key != "ExpectedEntries") valid = false;
            return true;
        }, "", true);
        openkey::ApplicationMemory next;
        if (!valid || !expected || !entries || *expected != applicationMemory_.encode() || !next.load(*entries)) return;
        // Serialize with automatic learning on the Fcitx event thread. Publish
        // only after persistence succeeds; never switch the active method here.
        ApplicationMemoryConfig saved;
        saved.entries.setValue(next.encode());
        if (!fcitx::safeSaveAsIni(saved, "conf/openkey-app-memory.conf")) return;
        applicationMemory_ = std::move(next);
    }
    void setConfig(const fcitx::RawConfig& raw) override {
        const auto previous = config_;
        config_.load(raw, true);
        openkey::MacroTable::Entries checkedMacros;
        openkey::ApplicationRules::Entries checkedRules;
        if (!openkey::MacroTable::parse(*config_.macroTable, checkedMacros) ||
            !openkey::ApplicationRules::parse(*config_.applicationRules, checkedRules)) {
            config_ = previous;
            return; // Reject the entire malformed patch; leave runtime/file untouched.
        }
        if (compositionConfigChanged(previous)) finishContexts();
        openkey::ApplicationRules::parse(*config_.applicationRules, appRules_);
        // Keep future/third-party keys when applying a partial settings update.
        if (*previous.rememberApplication != *config_.rememberApplication || *previous.gnomeIdentity != *config_.gnomeIdentity) {
            resetMemoryFocus();
            auto* ic = instance_->mostRecentInputContext();
            if (memoryEligible(ic)) {
                memoryFocus_ = ic;
                if (applicationMemory_.lookup(ic->program()).empty()) rememberApplication(ic);
            }
        }
        fcitx::RawConfig saved;
        fcitx::readAsIni(saved, "conf/openkey.conf");
        raw.visitSubItems([&saved](const fcitx::RawConfig& item, const std::string& path) {
            saved.setValueByPath(path, item.value());
            return true;
        }, "", true);
        config_.save(saved);
        if (!fcitx::safeSaveAsIni(saved, "conf/openkey.conf"))
            FCITX_WARN() << "OpenKey could not save configuration";
    }
    void reloadConfig() override {
        const auto previous = config_;
        fcitx::readAsIni(config_, "conf/openkey.conf");
        openkey::MacroTable::Entries macros;
        openkey::ApplicationRules::Entries rules;
        if (!openkey::MacroTable::parse(*config_.macroTable, macros) ||
            !openkey::ApplicationRules::parse(*config_.applicationRules, rules)) { config_ = previous; return; }
        if (compositionConfigChanged(previous)) finishContexts();
        openkey::ApplicationRules::parse(*config_.applicationRules, appRules_);
        loadApplicationMemory();
        if (*previous.rememberApplication != *config_.rememberApplication || *previous.gnomeIdentity != *config_.gnomeIdentity) {
            resetMemoryFocus();
            auto* ic = instance_->mostRecentInputContext();
            if (memoryEligible(ic)) memoryFocus_ = ic;
        }
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&factory_);
        dropNativeClear(event.inputContext(), "context-changed");
        state->nativeClearBlocked = false;
        state->active = true;
        state->lastReason.clear();
        // EN/VI comes back through activate. A recovery buffer left by the
        // previous method would swallow every following key.
        state->recovery.clear();
        clearDirect(*state);
        state->composition = openkey::Composition(entry.uniqueName() == "openkey-vni" ? vVNI :
            entry.uniqueName() == "openkey-simple-telex1" ? vSimpleTelex1 :
            entry.uniqueName() == "openkey-simple-telex2" ? vSimpleTelex2 : vTelex,
                                                  compositionOptions());
        auto* ic = event.inputContext();
        // Some clients (Chrome address bar, Qt) ignore the first IM edit until a
        // preedit/focus handshake. Send an empty client preedit when Direct.
        if (styleFor(ic) == InputStyle::Direct &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit)) {
            ic->inputPanel().reset();
            ic->inputPanel().setClientPreedit(fcitx::Text{});
            ic->updatePreedit();
        }
        update(ic);
    }

    void keyEvent(const fcitx::InputMethodEntry&, fcitx::KeyEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        { const auto rawKey = event.rawKey(); traceClear(ic, event.isRelease() ? "key-release" : "key-press", &rawKey); }
        // A key overtook the clear's ACK: resolve it first, in order.
        if (!event.isRelease() && !event.key().isModifier()) resolveNativeClear(ic, true);
        auto& composition = state.composition;
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) {
            diagnose(ic, "bypass:sensitive-field");
            clearDirect(state);
            state.recovery.clear();
            composition.clear();
            update(ic);
            return;
        }
        if (styleFor(ic) == InputStyle::NativeDirect && terminalDeleteKey(ic, event)) return;
        if (finishBoundary(ic, event)) return;
        if (styleFor(ic) == InputStyle::CommitOnly) {
            commitOnlyKey(ic, event);
            return;
        }
        if (styleFor(ic) == InputStyle::NativeDirect) {
            nativeDirectKey(ic, event);
            return;
        }
        if (!state.recovery.empty()) {
            if (event.isRelease() || event.key().isModifier()) return;
            const auto key = event.key();
            const auto modifiers = fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
            if (key.states() & modifiers) return;
            if (key.sym() == FcitxKey_Escape) state.recovery.clear();
            else if (key.sym() == FcitxKey_Return || key.sym() == FcitxKey_KP_Enter) {
                // Even explicit recovery is insertion only: require a valid
                // collapsed selection, never replace highlighted content.
                if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
                    !openkey::Snapshot::read(ic->surroundingText())) {
                    update(ic);
                    event.filterAndAccept();
                    return;
                }
                const auto recovered = std::move(state.recovery);
                state.recovery.clear();
                ic->commitString(recovered); // User explicitly confirms the new destination.
            } else if (key.sym() == FcitxKey_BackSpace) popCharacter(state.recovery);
            else {
                const auto ch = fcitx::Key::keySymToUnicode(key.sym());
                if (printable(ch) && openkey::codepoints(state.recovery).size() < 256)
                    state.recovery += fcitx::utf8::UCS4ToUTF8(ch);
                else return;
            }
            update(ic);
            event.filterAndAccept();
            return;
        }
        if (state.owned) {
            if (event.isRelease() || event.key().isModifier()) return;
            const auto ownedKey = event.key();
            const auto ownedSymbol = fcitx::Key::keySymToUnicode(ownedKey.sym());
            const bool ownedText = letterOrDigit(ownedSymbol);
            // The word is already in the document. Enter/Home/Space must reach the
            // chat box; do not accept them and do not delete the committed word.
            if (ownedSymbol == ' ' && !state.composition.empty()) {
                // The chat must receive Space. Keep the word so Backspace can
                // still apply a tone (coong Space Backspace j → cộng).
                state.previousWord = state.composition;
                state.trailingSpaces = " ";
                state.owned = false;
                state.retainOwned = true;
                state.composition.clear();
                state.primed = false;
                state.nativePrime = false;
                state.tracker.clear();
                return;
            }
            if (shortcutKey(ownedKey) || enterKey(ownedKey) || navigationKey(ownedKey) ||
                (!ownedText && ownedKey.sym() != FcitxKey_BackSpace)) {
                state.owned = false;
                state.retainOwned = false;
                state.ownedLen = 0;
                state.ownedSkip = 0;
                state.direct = false;
                state.primed = false;
                state.nativePrime = false;
                state.composition.clear();
                state.previousWord.reset();
                state.trailingSpaces.clear();
                state.tracker.clear();
                return;
            }
            if (ownedKey.sym() == FcitxKey_BackSpace) {
                if (!state.composition.backspace()) {
                    state.owned = false;
                    state.retainOwned = false;
                    state.ownedLen = 0;
                    state.ownedSkip = 0;
                    state.direct = false;
                    return;
                }
            } else if (!state.composition.append(static_cast<char>(ownedSymbol))) return;
            commitOwned(ic);
            event.filterAndAccept();
            return;
        }
        if (state.retainOwned && !event.isRelease() && !event.key().isModifier()) {
            const auto retained = event.key();
            if (fcitx::Key::keySymToUnicode(retained.sym()) == ' ') {
                if (state.trailingSpaces.size() < 32) state.trailingSpaces.push_back(' ');
                return;
            }
            if (retained.sym() == FcitxKey_BackSpace && !state.trailingSpaces.empty()) {
                state.trailingSpaces.pop_back();
                if (state.trailingSpaces.empty() && state.previousWord) {
                    state.composition = *state.previousWord;
                    state.previousWord.reset();
                    state.owned = true;
                    state.retainOwned = false;
                }
                return;
            }
            // A new word starts. The previous one already contains its Space.
            state.retainOwned = false;
            state.previousWord.reset();
            state.trailingSpaces.clear();
            state.primed = false;
        }
        if (state.direct && state.tracker.pending()) {
            const auto& pendingKey = event.rawKey();
            const auto pendingSymbol = fcitx::Key::keySymToUnicode(pendingKey.sym());
            // The native first key is still unACKed. commitString of each later
            // letter never appears in Cursor's chat once ee becomes ê. Show the
            // whole syllable as preedit now, instead of queueing or rewriting.
            if (state.nativePrime && !event.isRelease() && !pendingKey.isModifier() &&
                (letterOrDigit(pendingSymbol) || pendingKey.sym() == FcitxKey_BackSpace)) {
                const auto symbol = fcitx::Key::keySymToUnicode(event.key().sym());
                if (event.key().sym() == FcitxKey_BackSpace) {
                    if (!state.composition.backspace() || state.composition.empty()) {
                        state.composition.clear();
                        if (state.timeout) state.timeout->setEnabled(false);
                        state.tracker.clear();
                        state.queue.clear();
                        state.nativePrime = false;
                        state.primed = false;
                        state.direct = false;
                        return;
                    }
                } else if (!letterOrDigit(symbol) ||
                           !state.composition.append(static_cast<char>(symbol))) {
                    return;
                }
                promoteNativePrime(ic);
                event.filterAndAccept();
                return;
            }
            if (state.nativePrime && !event.isRelease() && !pendingKey.isModifier() &&
                pendingSymbol >= 32 && pendingSymbol < 127 && !letterOrDigit(pendingSymbol)) {
                // Comma/period/space must show up immediately. The native letter
                // is already in the document; do not queue the delimiter.
                state.tracker.clear();
                state.queue.clear();
                state.nativePrime = false;
                state.primed = false;
                state.direct = false;
                state.composition.clear();
                if (state.timeout) state.timeout->setEnabled(false);
                return;
            }
            if (composingKey(pendingKey, pendingSymbol)) {
                if (state.queue.size() >= 64) abandon(ic, "queue-overflow");
                else {
                    state.queue.push_back({pendingKey, event.isRelease(), event.time()});
                    event.filterAndAccept();
                }
                return;
            }
            if (event.isRelease() || pendingKey.isModifier()) return;
            // Boundary key while an edit is in flight: ACK optimistically, flush
            // composing keys, end Direct ownership, then leave the key unaccepted
            // so Wayland/app deliver Enter/nav/shortcut natively.
            diagnose(ic, "pending-boundary");
            state.tracker.assumeAcked();
            if (state.timeout) state.timeout->setEnabled(false);
            flushQueue(ic);
            endDirectWord(ic, pendingKey);
            return;
        }
        if (event.isRelease()) return;
        const auto key = event.key();
        // Modifier keys alone must not commit (e.g. pressing Shift for capitals).
        if (key.isModifier()) return;
        const auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        const bool textKey = letterOrDigit(symbol);
        const bool terminalPreedit = ibusClient(ic) && styleFor(ic) == InputStyle::Direct &&
            directRewriteUnsafe(ic) && (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit);
        state.hiddenPreedit = ibusClient(ic) && styleFor(ic) == InputStyle::Direct && appendOnlyClient(ic);
        if (terminalPreedit && state.plainCommit) {
            // The field changed under a Direct word (editor -> terminal). That
            // word is already in the document; start the next one in preedit.
            composition.clear();
            state.placed.clear();
            state.ownedLen = 0;
            state.plainCommit = false;
            update(ic);
        }
        if (terminalPreedit && composition.empty() && letterOrDigit(fcitx::Key::keySymToUnicode(key.sym())))
            diagnose(ic, state.hiddenPreedit ? "fallback:commit-only-terminal" : "fallback:preedit-terminal");
        if (ibusClient(ic) && styleFor(ic) == InputStyle::Direct && !terminalPreedit) {
            // Keep the composed word in the document and the preedit empty.
            auto showWord = [this, ic]() {
                auto& word = *ic->propertyFor(&factory_);
                const auto nextText = word.composition.text();
                const auto previous = openkey::codepoints(word.placed);
                const auto next = openkey::codepoints(nextText);
                struct Glyph { char32_t nfc; int tone; std::size_t index; std::size_t count; };
                const auto glyphs = [](const std::u32string& text) {
                    std::vector<Glyph> out;
                    for (std::size_t i = 0; i < text.size();) {
                        const auto cluster = openkey::readVietCluster(text, i);
                        out.push_back({cluster.nfc, cluster.tone, i, cluster.count});
                        i += cluster.count ? cluster.count : 1;
                    }
                    return out;
                };
                const auto oldG = glyphs(previous);
                const auto newG = glyphs(next);
                std::size_t g = 0;
                while (g < oldG.size() && g < newG.size() && oldG[g].nfc == newG[g].nfc) ++g;
                const bool sensitive = static_cast<bool>(ic->capabilityFlags() &
                    fcitx::CapabilityFlag::PasswordOrSensitive);
                auto publish = [&](unsigned remove, const std::u32string& insert, const std::u32string& actual) {
                    word.emitting = true;
                    word.forcePreedit = false;
                    word.direct = false;
                    word.owned = false;
                    word.plainCommit = !actual.empty();
                    if (remove)
                        ic->deleteSurroundingText(-static_cast<int>(remove), remove);
                    if (!insert.empty() && !sensitive) ic->commitString(openkey::utf8(insert));
                    word.emitting = false;
                    word.placed = openkey::utf8(actual);
                    word.ownedLen = static_cast<unsigned>(actual.size());
                    update(ic);
                };
                if (g == oldG.size()) {
                    std::u32string insert;
                    if (g < newG.size())
                        insert.assign(next.begin() + static_cast<std::ptrdiff_t>(newG[g].index), next.end());
                    auto actual = previous;
                    actual += insert;
                    publish(0, insert, actual);
                    return;
                }
                // alpha.25 kept ă/ê/ô/ơ/ư and attached a combining tone (băngằng
                // workaround). Users saw detached marks (mẻ, tiếng). Always write
                // the precomposed letter; residual risk documented in QA (alpha.31).
                // Chrome ignores every delete, so o stays and ô is written after it
                // (goô). The vowel is still the last letter: attach the shape or tone
                // mark and do not delete. Cursor keeps the precomposed letter.
                if (asciiDeleteUnreliable(ic) && g < oldG.size() && g < newG.size() &&
                    oldG[g].index + oldG[g].count == previous.size()) {
                    char32_t oldStem = 0, newStem = 0;
                    const int oldTone = openkey::vietTone(oldG[g].nfc, oldStem);
                    const int newTone = openkey::vietTone(newG[g].nfc, newStem);
                    const auto oldAscii = oldTone >= 0 ? openkey::vietAscii(oldStem) : 0;
                    const auto newAscii = newTone >= 0 ? openkey::vietAscii(newStem) : 0;
                    if (oldAscii && oldAscii == newAscii && oldAscii <= 127) {
                        std::u32string desired;
                        desired.push_back(oldAscii);
                        if (const auto shape = openkey::vietShapeMark(newStem)) desired.push_back(shape);
                        if (const auto mark = openkey::vietMark(newTone)) desired.push_back(mark);
                        const auto oldCount = oldG[g].count;
                        bool prefix = oldCount <= desired.size();
                        for (std::size_t n = 0; prefix && n < oldCount; ++n)
                            prefix = previous[oldG[g].index + n] == desired[n];
                        if (prefix) {
                            std::u32string insert(desired.begin() + static_cast<std::ptrdiff_t>(oldCount),
                                                  desired.end());
                            const auto tailAt = newG[g].index + newG[g].count;
                            insert.append(next.begin() + static_cast<std::ptrdiff_t>(tailAt), next.end());
                            auto actual = previous;
                            actual += insert;
                            publish(0, insert, actual);
                            return;
                        }
                    }
                }
                std::size_t same = 0;
                while (same < previous.size() && same < next.size() && previous[same] == next[same]) ++same;
                const auto remove = static_cast<unsigned>(previous.size() - same);
                std::u32string insert(next.begin() + static_cast<std::ptrdiff_t>(same), next.end());
                std::u32string actual(previous.begin(), previous.begin() + static_cast<std::ptrdiff_t>(same));
                actual += insert;
                publish(remove, insert, actual);
            };
            auto releaseWord = [this, ic](bool erase) {
                auto& word = *ic->propertyFor(&factory_);
                if (erase && word.ownedLen) {
                    word.emitting = true;
                    ic->deleteSurroundingText(-static_cast<int>(word.ownedLen), word.ownedLen);
                    word.emitting = false;
                }
                word.composition.clear();
                word.placed.clear();
                word.ownedLen = 0;
                word.plainCommit = false;
                word.owned = false;
                word.forcePreedit = false;
                word.direct = false;
                word.bypass = false;
                update(ic);
            };
            if (enterKey(key)) {
                // End ALL ownership before the native Enter reaches the app.
                // On the next line, do not begin with commitString: xterm.js
                // on Wayland can ignore IM-only text until a native text key.
                releaseWord(false);
                clearDirect(state);
                state.ibusAfterEnter = true;
                diagnose(ic, "ibus:enter-native-prime-armed");
                return;
            }
            if (shortcutKey(key)) {
                releaseWord(false);
                clearDirect(state);
                return;
            }
            if (state.ibusNativeWord) {
                // No surrounding ACK for the first native character. Never
                // pretend we own it or delete text at an unconfirmed caret.
                if (!textKey && key.sym() != FcitxKey_BackSpace) {
                    clearDirect(state);
                    composition.clear();
                }
                return;
            }
            std::optional<openkey::Snapshot> nativeAppendBase;
            if (state.ibusPrimeExpected) {
                const auto snapshot = openkey::Snapshot::read(ic->surroundingText());
                if (!snapshot || !(*snapshot == *state.ibusPrimeExpected)) {
                    clearDirect(state);
                    composition.clear();
                    state.ibusNativeWord = textKey || key.sym() == FcitxKey_BackSpace;
                    diagnose(ic, "ibus:post-enter-native-unconfirmed");
                    return;
                }
                nativeAppendBase = snapshot;
                state.ibusPrimeExpected.reset();
            }
            // Enter is not the only entry point: focus from another app or
            // from the editor may start a word without any Enter event here.
            // Begin ordinary ASCII words natively too, while retaining real
            // first-key transformations (e.g. standalone Telex w -> ư).
            bool freshNativeWord = false;
            if (textKey && composition.empty()) {
                auto first = composition;
                freshNativeWord = first.append(static_cast<char>(symbol)) &&
                    first.text() == fcitx::utf8::UCS4ToUTF8(symbol);
            }
            if (state.ibusAfterEnter || freshNativeWord) {
                state.ibusAfterEnter = false;
                if (textKey) {
                    const auto snapshot = openkey::Snapshot::read(ic->surroundingText());
                    clearDirect(state);
                    composition.clear();
                    // GNOME may advertise SurroundingText late after focus.
                    // The cached baseline alone never authorizes a rewrite:
                    // the NEXT key must see the exact native result, or the
                    // rest of this word stays raw. No timeout/optimistic ACK.
                    if (!snapshot || !composition.append(static_cast<char>(symbol)) ||
                        composition.text() != fcitx::utf8::UCS4ToUTF8(symbol)) {
                        composition.clear();
                        state.ibusNativeWord = true;
                        diagnose(ic, "ibus:native-start-unconfirmed");
                        return;
                    }
                    auto expected = *snapshot;
                    expected.text.insert(expected.cursor, 1, static_cast<char32_t>(symbol));
                    ++expected.cursor;
                    expected.anchor = expected.cursor;
                    state.ibusPrimeExpected = std::move(expected);
                    state.placed = composition.text();
                    state.ownedLen = 1;
                    state.plainCommit = true;
                    diagnose(ic, "ibus:word-prime-native");
                    // Not accept+forwardKey: leave the original event native.
                    return;
                }
            }
            if (key.sym() == FcitxKey_Escape && !composition.empty()) {
                releaseWord(true);
                event.filterAndAccept();
                return;
            }
            if (key.sym() == FcitxKey_BackSpace && !composition.empty()) {
                if (!composition.backspace()) composition.clear();
                state.bypass = false;
                state.nativePrime = false;
                state.repairNativePrime = false;
                showWord();
                event.filterAndAccept();
                return;
            }
            if (textKey) {
                if (!composition.append(static_cast<char>(symbol))) {
                    releaseWord(false);
                    return;
                }
                // alpha.32 only passed the first post-Enter character natively.
                // Even an unchanged suffix ('s' in 'ls') then re-entered the
                // IM commit path and could disappear in xterm.js. Keep the
                // original events native for exact ASCII appends. A genuine
                // Telex/VNI transformation still uses the existing rewrite
                // path; do not silently force the entire word to English.
                if (nativeAppendBase &&
                    composition.text() == state.placed + fcitx::utf8::UCS4ToUTF8(symbol)) {
                    auto expected = *nativeAppendBase;
                    expected.text.insert(expected.cursor, 1, static_cast<char32_t>(symbol));
                    ++expected.cursor;
                    expected.anchor = expected.cursor;
                    state.ibusPrimeExpected = std::move(expected);
                    state.placed = composition.text();
                    state.ownedLen = static_cast<unsigned>(openkey::codepoints(state.placed).size());
                    state.plainCommit = true;
                    diagnose(ic, "ibus:post-enter-append-native");
                    return;
                }
                state.bypass = false;
                state.nativePrime = false;
                state.repairNativePrime = false;
                state.preeditDrifted = false;
                if (state.timeout) state.timeout->setEnabled(false);
                showWord();
                event.filterAndAccept();
                return;
            }
            if (!composition.empty()) {
                // The word is already on screen. Space/Enter/punctuation pass through.
                releaseWord(false);
                return;
            }
        }
        if (state.bypass) {
            if (!textKey) state.bypass = false;
            return;
        }
        if (!terminalPreedit && !state.forcePreedit && !state.direct && composition.empty() &&
            styleFor(ic) == InputStyle::Direct &&
            !surroundingTextUnreliable(ic) &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) &&
            openkey::Snapshot::read(ic->surroundingText(), true)) state.direct = true;
        else if (!state.direct && styleFor(ic) == InputStyle::Direct && surroundingTextUnreliable(ic) &&
                 composition.empty())
            diagnose(ic, "fallback:preedit-unreliable-surrounding");
        if (state.direct) {
            // shortcutKey must win over letterOrDigit (Ctrl+A still has symbol 'a').
            if (shortcutKey(key) || (!textKey && (enterKey(key) || navigationKey(key)))) {
                endDirectWord(ic, key);
                return; // unaccepted → native delivery
            }
            const auto outcome = directKey(ic, key, textKey, symbol);
            if (outcome == DirectOutcome::Consumed) {
                event.filterAndAccept();
                return;
            }
            if (state.direct || state.bypass ||
                ((ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) &&
                 openkey::Snapshot::read(ic->surroundingText(), true))) return;
            // If the client withdrew capability/context while idle, process
            // this same key in Preedit, rather than lose the first keystroke.
        }
        if (styleFor(ic) == InputStyle::Direct && !state.forcePreedit && !surroundingTextUnreliable(ic) &&
            !terminalPreedit)
            diagnose(ic, !composition.empty() ? "fallback:preedit-word-in-progress" :
                ((ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "fallback:invalid-surrounding-text" : "fallback:no-surrounding-capability"));
        // Direct requested but the client can neither surround-edit nor show
        // preedit (e.g. many terminals): do not steal keys into an invisible
        // server composition. Preedit style still uses server-side preedit.
        // A client that CAN show preedit must still convert (chees → chế).
        if (styleFor(ic) == InputStyle::Direct && !state.forcePreedit &&
            !(ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit)) {
            diagnose(ic, "fallback:passthrough-no-preedit-capability");
            composition.clear();
            update(ic);
            return;
        }
        // The caret left the word (Enter/Home/Escape) after surrounding text
        // moved. Do not commit the syllable into the new place.
        // ibus Enter sends the chat message, so the unfinished word is committed
        // rather than discarded when surrounding text moved.
        if (state.preeditDrifted && (navigationKey(key) || shortcutKey(key) ||
                                     key.sym() == FcitxKey_Escape ||
                                     (enterKey(key) && !ibusClient(ic)))) {
            composition.clear();
            state.preeditDrifted = false;
            state.forcePreedit = false;
            state.repairNativePrime = false;
            state.nativePrimeChar = 0;
            state.nativePrimeBase.reset();
            update(ic);
            if (key.sym() == FcitxKey_Escape) event.filterAndAccept();
            return;
        }
        if (shortcutKey(key)) {
            commit(ic);
            return;
        }
        if (key.sym() == FcitxKey_Escape && !composition.empty()) {
            composition.clear();
            update(ic);
            event.filterAndAccept();
            return;
        }
        if (composition.full()) commit(ic);
        if (key.sym() == FcitxKey_BackSpace) {
            if (composition.backspace()) {
                state.preeditDrifted = false;
                update(ic);
                event.filterAndAccept();
            }
            return;
        }
        if (symbol < 128 && composition.append(static_cast<char>(symbol))) {
            state.preeditDrifted = false;
            update(ic);
            event.filterAndAccept();
            return;
        }
        const auto boundary = openkey::Snapshot::read(ic->surroundingText(), true);
        if (boundary && boundary->cursor != boundary->anchor) return;
        // Space after a syllable that was promoted from an unACKed Direct key.
        // Keep the word so Backspace can still add a tone (coong Space Backspace j).
        if (state.forcePreedit && symbol == ' ' && !composition.empty()) {
            state.previousWord = state.composition;
            state.trailingSpaces = " ";
            const auto wordChars = openkey::codepoints(state.composition.text()).size();
            commit(ic);
            // ibus will not delete the committed word on Backspace, so do not
            // keep it for a tone correction. The application handles Backspace.
            if (!ibusClient(ic)) {
                state.ownedLen = static_cast<unsigned>(wordChars);
                state.ownedSkip = 0;
                state.retainOwned = true;
            }
            return;
        }
        // Commit before passing Space/Enter/punctuation/navigation to the app.
        commit(ic);
    }

    void reset(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        // Direct text is already in the application; never erase or re-commit it.
        // Drop any recovery buffer: reset means the client left the composition.
        // A text editor also resets after our own commitString (hoo → hô). That
        // reset is not a new word: dropping the syllable there makes the tone
        // key apply only to what follows (hôị).
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        dropNativeClear(ic, "context-reset");
        if (styleFor(ic) == InputStyle::NativeDirect && state.nativeKeepOnReset) {
            state.nativeKeepOnReset = false;
            state.nativeResetHeld = true;
            return;
        }
        settle(ic);
        state.recovery.clear();
        clearDirect(state);
        state.composition.clear();
        update(ic);
    }

    void deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        dropNativeClear(ic, "context-changed");
        const bool sensitive = static_cast<bool>(ic->capabilityFlags() &
            fcitx::CapabilityFlag::PasswordOrSensitive);
        // Focus-out with client preedit: the frontend commits that preedit.
        // Committing again duplicates the word.
        // plainCommit text is already in the document. Committing it again duplicates.
        const bool clientHoldsPreedit = event.type() == fcitx::EventType::InputContextFocusOut &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit) && !state.direct &&
            !state.plainCommit && !state.hiddenPreedit && styleFor(ic) != InputStyle::CommitOnly;
        if (state.direct) settle(ic);
        else if (!sensitive && !clientHoldsPreedit && !state.composition.empty() && !state.plainCommit) {
            // Switching to English must not delete the character before the caret.
            state.repairNativePrime = false;
            commit(ic);
        } else {
            state.composition.clear();
            update(ic);
        }
        state.recovery.clear();
        clearDirect(state);
        state.composition.clear();
        state.active = false;
        update(ic);
    }

private:
    bool terminalDeleteContext(fcitx::InputContext* ic) const {
        if (!*config_.terminalDeleteFix || styleFor(ic) != InputStyle::NativeDirect ||
            (ic->capabilityFlags() & fcitx::CapabilityFlags{fcitx::CapabilityFlag::Url,
                fcitx::CapabilityFlag::Email, fcitx::CapabilityFlag::PasswordOrSensitive,
                fcitx::CapabilityFlag::Disable})) return false;
        auto program = ic->program();
        const auto slash = program.find_last_of('/');
        if (slash != std::string::npos) program.erase(0, slash + 1);
        for (auto& ch : program) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        if (program.ends_with(".desktop")) program.resize(program.size() - 8);
        // Never treat gnome-shell, Electron in general or an unknown identity
        // as Code. Cached GNOME application memory is not proof of focus.
        if (program != "code" && program != "code-insiders" && program != "cursor") return false;
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText());
        if (!snapshot) return false; // Missing/invalid text is NOT an empty report.
        return std::all_of(snapshot->text.begin(), snapshot->text.end(), [](char32_t ch) {
            return ch == U' ' || ch == U'\t' || ch == U'\n' || ch == U'\r' || ch == U'\f' || ch == U'\v';
        });
    }

    // DEL goes through xterm's insertText -> PTY path. This is deliberately
    // opt-in: only canonical UTF-8 line discipline can erase codepoints safely.
    bool terminalDeleteKey(fcitx::InputContext* ic, fcitx::KeyEvent& event) {
        auto& state = *ic->propertyFor(&factory_);
        if (!terminalDeleteContext(ic)) {
            if (!state.terminalWord.empty()) {
                const auto snapshot = openkey::Snapshot::read(ic->surroundingText());
                const auto word = openkey::codepoints(state.terminalWord);
                // An initially empty editor also matches the textarea signature.
                // When it reports the committed first letter, hand the same
                // syllable to NativeDirect, without DEL or losing its first key.
                if (snapshot && snapshot->cursor >= word.size() &&
                    snapshot->text.substr(snapshot->cursor - word.size(), word.size()) == word) {
                    state.nativeExpected = snapshot;
                    state.nativeObserved = snapshot;
                    state.placed = state.terminalWord;
                    state.terminalWord.clear();
                    state.plainCommit = false;
                } else {
                    state.composition.clear();
                    clearDirect(state);
                }
            }
            return false;
        }
        if (event.isRelease() || event.key().isModifier()) return true;
        const auto key = event.key();
        const auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        const bool textKey = letterOrDigit(symbol);
        const bool backspace = key.sym() == FcitxKey_BackSpace;
        if (shortcutKey(key) || (!textKey && !backspace)) {
            // Boundaries stay native. No reopening earlier words across spaces,
            // cursor movement, Enter/Tab, shortcuts or focus/reset events.
            state.composition.clear();
            clearDirect(state);
            return true;
        }
        if (state.terminalWord.empty()) {
            // Do not adopt a native prefix, even if a late spaces-only report
            // arrives mid-word. We may erase only our own successful commits.
            state.composition.clear();
            clearDirect(state);
            if (backspace) return true;
        }
        auto candidate = state.composition;
        if (!(backspace ? candidate.backspace() : candidate.append(static_cast<char>(symbol)))) {
            state.composition.clear();
            clearDirect(state);
            return true;
        }
        const auto before = openkey::codepoints(state.terminalWord);
        const auto after = openkey::codepoints(candidate.text());
        std::size_t same = 0;
        while (same < before.size() && same < after.size() && before[same] == after[same]) ++same;
        const auto payload = std::string(before.size() - same, '\x7f') +
            openkey::utf8(after.substr(same));
        state.composition = std::move(candidate);
        state.terminalWord = state.composition.text();
        state.plainCommit = !state.terminalWord.empty();
        state.emitting = true;
        if (!payload.empty()) ic->commitString(payload);
        state.emitting = false;
        event.filterAndAccept();
        return true;
    }

    // Native keys own insertion, selection replacement and ordinary Backspace.
    // Only a real engine transformation uses an IM edit, after exact validation
    // of the entire expected document/caret. Never queue, forward or show preedit.
    void nativeDirectKey(fcitx::InputContext* ic, fcitx::KeyEvent& event) {
        if (event.isRelease() || event.key().isModifier()) return;
        auto& state = *ic->propertyFor(&factory_);
        const auto key = event.key();
        const auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        const bool textKey = letterOrDigit(symbol);
        const bool backspace = key.sym() == FcitxKey_BackSpace;
        const bool boundary = shortcutKey(key) || (!textKey && !backspace);
        // A letter after an unreported native edit starts a new word whose
        // baseline is the client's next update, never the stale cache.
        const bool adopt = textKey && !shortcutKey(key) && state.nativeStale && !state.nativeClear;
        if (adopt) {
            diagnose(ic, "native:stale-cache", "adopt-next-update");
            state.composition.clear();
            clearDirect(state);
        }
        // Chrome reports nothing after these (see nativeStale). Mark before any
        // early return; a later notification clears it.
        if (shortcutKey(key) || (!textKey && symbol != U' ')) state.nativeStale = true;
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        const bool cacheWiped = snapshot && snapshot->text.empty() && snapshot->cursor == snapshot->anchor &&
            !state.composition.empty() && !state.placed.empty() &&
            (state.nativeKeepOnReset || state.nativeResetHeld);
        if (state.nativeResetHeld && !cacheWiped && !nativeResetStillHere(state, snapshot)) {
            state.composition.clear();
            state.placed.clear();
            state.nativeExpected.reset();
            state.nativeObserved.reset();
            state.nativeCompletion.reset();
            state.nativeAwaitingUpdate = false;
            state.nativeRawWord = false;
            state.nativeResetHeld = false;
        } else if (state.nativeResetHeld) {
            state.nativeResetHeld = false;
        }
        // The native keys are already in the application. A surrounding-text
        // notification can still show the previous baseline (focus into Cursor
        // is often late). Keep that prediction and convert oo -> ô; do not
        // freeze the rest of the word as raw ASCII. An unrelated document is
        // not this baseline and must not authorize the edit.
        const bool cacheUnmoved = state.nativeAwaitingUpdate && state.nativeExpected &&
            state.nativeObserved && snapshot && *snapshot == *state.nativeObserved;
        const bool catchingUp = state.nativeAwaitingUpdate && state.nativeExpected && state.nativeObserved &&
            snapshot && nativeStillTyping(*state.nativeObserved, *state.nativeExpected, *snapshot);
        bool predict = cacheUnmoved || catchingUp || cacheWiped;
        const bool urlSelection = snapshot && state.nativeExpected &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
            openkey::selectedUrlCompletion(*state.nativeExpected, *snapshot);
        if (state.nativeAwaitingUpdate && !predict && !urlSelection && state.nativeExpected && snapshot &&
            snapshot->cursor == snapshot->anchor && !(*snapshot == *state.nativeExpected)) {
            const bool typing = !state.composition.empty() && !state.placed.empty();
            const bool ours = nativeOwnEcho(*state.nativeExpected, *snapshot);
            const bool elsewhere = nativeSuffixElsewhere(openkey::codepoints(state.placed), *snapshot);
            const bool foreign = typing && !snapshot->text.empty() && !ours && !elsewhere &&
                !(ic->capabilityFlags() & fcitx::CapabilityFlag::Url);
            const bool caughtUp = typing && elsewhere &&
                !nativeExpectedSuffix(*state.nativeExpected, *snapshot);
            if (caughtUp) {
                state.nativeExpected = snapshot;
                state.nativeObserved = snapshot;
                state.nativeAwaitingUpdate = false;
            } else if (typing && (ours || foreign)) {
                predict = true;
            } else if (!foreign) {
                state.composition.clear();
                state.placed.clear();
                state.nativeExpected.reset();
                state.nativeObserved.reset();
                state.nativeCompletion.reset();
                state.nativeAwaitingUpdate = false;
                state.nativeRawWord = false;
            } else {
                predict = true;
            }
        }
        const bool completion = snapshot && state.nativeExpected && state.nativeCompletion &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
            *snapshot == *state.nativeCompletion &&
            openkey::selectedUrlCompletion(*state.nativeExpected, *snapshot);
        const char* unsupported = !(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
            ? "no-surrounding-capability" : appendOnlyClient(ic) ? "terminal-context" :
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Email) ? "email-context" :
            surroundingTextUnreliable(ic) ? "unreliable-program" : nullptr;
        // Url is a content hint, not proof that the client ignores deletions.
        // NativeDirect never creates a client composition; let URL fields use
        // the same exact-ACK path as web inputs. Legacy Direct's URL workaround
        // stays separate. A client that drops an edit cannot authorize the next.
        const bool supported = !unsupported;
        auto forget = [&]() {
            state.composition.clear();
            clearDirect(state);
            update(ic);
        };
        // GNOME omits SurroundingText on the first keys after focus (chat is
        // 1048594, then later 1048658). The letters are already in the field.
        // A later key that actually changes the syllable replaces that native
        // prefix, so ddang/tieengs do not stay as raw Telex until Space.
        auto flushUnconfirmed = [&](bool rewrite) {
            if (textKey) {
                auto candidate = state.composition;
                if (!candidate.append(static_cast<char>(symbol))) {
                    state.composition.clear();
                    state.placed.clear();
                    state.plainCommit = false;
                    return;
                }
                const auto after = candidate.text();
                const auto extra = fcitx::utf8::UCS4ToUTF8(symbol);
                if (!rewrite || state.placed.empty() || after == state.placed + extra) {
                    state.composition = std::move(candidate);
                    state.placed.append(extra);
                    state.plainCommit = true;
                    return;
                }
                const auto remove = static_cast<unsigned>(openkey::codepoints(state.placed).size());
                state.emitting = true;
                if (remove) ic->deleteSurroundingText(-static_cast<int>(remove), remove);
                if (!after.empty()) ic->commitString(after);
                state.emitting = false;
                state.composition = std::move(candidate);
                state.placed = after;
                state.plainCommit = true;
                event.filterAndAccept();
                return;
            }
            if (backspace) {
                if (!state.composition.backspace()) state.composition.clear();
                auto raw = openkey::codepoints(state.placed);
                if (!raw.empty()) raw.pop_back();
                state.placed = openkey::utf8(raw);
                state.plainCommit = !state.placed.empty();
                return;
            }
            if (rewrite && !state.placed.empty() && state.composition.text() != state.placed) {
                const auto after = state.composition.text();
                const auto remove = static_cast<unsigned>(openkey::codepoints(state.placed).size());
                state.emitting = true;
                if (remove) ic->deleteSurroundingText(-static_cast<int>(remove), remove);
                if (!after.empty()) ic->commitString(after);
                state.emitting = false;
            }
            state.composition.clear();
            state.placed.clear();
            state.plainCommit = false;
        };
        const bool lateCapability = unsupported &&
            std::string_view(unsupported) == "no-surrounding-capability";
        if (lateCapability && !state.nativeRawWord) {
            diagnose(ic, "native:passthrough", "awaiting-surrounding-capability");
            flushUnconfirmed(false);
            return;
        }
        if (supported && snapshot && !state.nativeExpected && !state.placed.empty() && !state.nativeRawWord) {
            const auto raw = openkey::codepoints(state.placed);
            const bool landed = !raw.empty() && snapshot->cursor == snapshot->anchor &&
                snapshot->cursor >= raw.size() &&
                snapshot->text.compare(snapshot->cursor - raw.size(), raw.size(), raw) == 0;
            if (!landed) {
                diagnose(ic, "native:passthrough", "awaiting-surrounding-text");
                flushUnconfirmed(false);
                return;
            }
            state.nativeExpected = snapshot;
            state.nativeObserved = snapshot;
            state.nativeAwaitingUpdate = false;
        }
        const bool liveNoise = state.nativeExpected && snapshot && !state.composition.empty() &&
            !state.placed.empty() && snapshot->cursor == snapshot->anchor && !snapshot->text.empty() &&
            !(*state.nativeExpected == *snapshot) && !predict && !completion &&
            !nativeOwnEcho(*state.nativeExpected, *snapshot) &&
            !nativeSuffixElsewhere(openkey::codepoints(state.placed), *snapshot) &&
            !(ic->capabilityFlags() & fcitx::CapabilityFlag::Url);
        // A full-document echo that still starts like this edit (xã\nho while
        // the word is already hô) is the same syllable. Clearing it made the
        // following keys a new word: ij → ị and ngj → côngj.
        // Equal cache text is not an echo: a caller can write the prediction
        // into the cache without a surrounding notification, and that must not
        // authorize the next rewrite.
        const bool ownEcho = snapshot && state.nativeExpected && !state.composition.empty() &&
            !state.placed.empty() && !(*snapshot == *state.nativeExpected) &&
            nativeOwnEcho(*state.nativeExpected, *snapshot);
        if (liveNoise || ownEcho) predict = true;
        if (!supported || !snapshot || state.nativeRawWord ||
            (state.nativeAwaitingUpdate && !predict) ||
            (state.nativeExpected && !(*state.nativeExpected == *snapshot) && !completion && !predict)) {
            const bool rawWord = !boundary;
            const char* cause = unsupported ? unsupported : !snapshot ? "invalid-surrounding" :
                state.nativeRawWord ? "raw-word-in-progress" :
                state.nativeAwaitingUpdate ? "awaiting-surrounding-update" : "snapshot-mismatch";
            diagnose(ic, "native:passthrough", cause);
            forget();
            state.nativeRawWord = rawWord;
            return;
        }
        if (shortcutKey(key) || (boundary && key.sym() == FcitxKey_Escape) ||
            (completion && navigationKey(key))) {
            // Text already belongs to the app: Escape never erases it, and a
            // shortcut must not rewrite it at a possibly new selection.
            forget();
            return;
        }
        if (completion && backspace) {
            // Native Backspace removes the selected suggestion, not the last
            // typed letter. Preserve the engine history and await that update.
            state.nativeCompletion.reset();
            state.nativeAwaitingUpdate = true;
            update(ic);
            return;
        }
        // Only the selected suffix is extra. Native insertion replaces it.
        // No asynchronous update alone ever edits or deletes the selection.
        const auto editBase = (completion || predict) ? *state.nativeExpected : *snapshot;
        if (backspace && state.placed != state.composition.text() &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Url)) {
            // Raw letters held behind a URL completion (below) are what the
            // user sees; Backspace removes one of them, not the engine syllable.
            forget();
            return;
        }
        if (state.composition.full()) forget();
        auto candidate = state.composition;
        if (textKey) candidate.append(static_cast<char>(symbol));
        else if (backspace && !candidate.backspace()) {
            forget();
            return;
        }
        const auto before = openkey::codepoints(state.placed);
        auto after = openkey::codepoints(candidate.text());
        const auto rawChar = static_cast<char32_t>(symbol);
        // First key always native, even w -> ư: retain the rendered candidate
        // but track the actual w. A following key/boundary can convert it only
        // after the client confirms where the native character landed.
        const bool initial = textKey && !state.nativeExpected;
        bool native = initial;
        if (initial) after = std::u32string(1, rawChar);
        else if (textKey) native = after == before + std::u32string(1, rawChar);
        else if (backspace && !before.empty()) native = after == before.substr(0, before.size() - 1);
        else if (boundary) native = after == before;
        if (completion && !native) {
            // Chrome's omnibox completion is a REVERSED selection (anchor at the
            // end, caret after the typed text). Chrome 157 on Wayland measures an
            // IM deletion from the anchor (zwp_text_input_v3.cc), rejects the
            // range for not covering the selection and drops it, then the commit
            // replaces the selection: goo -> goô. A plain commit is safe, but
            // anything that must delete owned letters keeps the native key and
            // holds the raw letters; a later key or boundary with a confirmed
            // plain caret repairs them through the ordinary verified edit.
            std::size_t common = 0;
            while (common < before.size() && common < after.size() && before[common] == after[common]) ++common;
            const auto insert = after.substr(common);
            if (common < before.size() && textKey && rawChar < 128 && !insert.empty() && !state.nativeClearBlocked &&
                editBase.cursor == editBase.anchor && editBase.cursor == editBase.text.size() &&
                editBase.cursor >= before.size() &&
                editBase.text.compare(editBase.cursor - before.size(), before.size(), before) == 0) {
                // No deletion reaches the typed letters while the suggestion is
                // selected, and GNOME 50 never delivers a forwarded key to Chrome
                // (alpha.4 trace: no update in 150 ms after a forwarded BackSpace).
                // A plain commit does work: it replaces the selection. So commit
                // the new tail first (go|ogle.com -> goô), and once the client
                // reports exactly that with a collapsed caret, finish with the
                // ordinary caret delete + commit (goô -> gô).
                auto stage = editBase;
                stage.text += insert;
                stage.cursor = stage.anchor = static_cast<unsigned>(stage.text.size());
                state.composition = std::move(candidate);
                state.nativeCompletion.reset();
                state.placed = openkey::utf8(before + insert);
                state.nativeObserved = editBase;
                state.nativeExpected = stage;
                state.nativeAwaitingUpdate = true;
                state.nativeKeepOnReset = true;
                state.plainCommit = true;
                state.nativeClear = State::NativeClear{editBase, stage, after,
                    static_cast<unsigned>(before.size() - common)};
                diagnose(ic, "native:url-completion-clear", "verified-prefix-selection");
                if (*config_.diagnostics)
                    state.clearTrace = State::ClearTrace{editBase, fcitx::now(CLOCK_MONOTONIC), 0, 12};
                traceClear(ic, "armed");
                event.filterAndAccept();
                state.emitting = true;
                ic->commitString(openkey::utf8(insert));
                state.emitting = false;
                traceClear(ic, "committed");
                resolveNativeClear(ic, false); // a synchronous client already reported
                update(ic);
                return;
            }
            if (common < before.size()) {
                diagnose(ic, "native:url-completion-raw", "selection-blocks-delete");
                if (textKey) after = before + std::u32string(1, rawChar);
                else after = before;
                native = true;
            }
        }
        if (!initial && (editBase.cursor != editBase.anchor || editBase.cursor < before.size() ||
                editBase.text.substr(editBase.cursor - before.size(), before.size()) != before)) {
            forget();
            state.nativeRawWord = !boundary;
            return;
        }
        openkey::Snapshot expected = editBase;
        unsigned removed = 0;
        std::u32string insert;
        if (initial) {
            const auto at = std::min(expected.cursor, expected.anchor);
            expected.text.replace(at, std::max(expected.cursor, expected.anchor) - at, after);
            expected.cursor = expected.anchor = at + 1;
        } else {
            std::size_t common = 0;
            while (common < before.size() && common < after.size() && before[common] == after[common]) ++common;
            removed = static_cast<unsigned>(before.size() - common);
            insert = after.substr(common);
            expected.text.replace(expected.cursor - removed, removed, insert);
            expected.cursor = expected.anchor = expected.cursor - removed + static_cast<unsigned>(insert.size());
        }
        if (completion && !native) diagnose(ic, "native:url-completion-edit", "verified-prefix-selection");
        if (native || initial || after != before) state.nativeCompletion.reset();
        state.composition = std::move(candidate);
        state.placed = openkey::utf8(after);
        if (!predict) state.nativeObserved = editBase;
        state.nativeExpected = std::move(expected);
        state.nativeAwaitingUpdate = initial || after != before;
        if (initial && adopt) state.nativeAdopt = rawChar;
        state.plainCommit = true; // Never re-commit this word on focus/config changes.
        if (!native) {
            state.nativeKeepOnReset = true;
            state.emitting = true;
            if (removed) ic->deleteSurroundingText(-static_cast<int>(removed), removed);
            if (!insert.empty()) ic->commitString(openkey::utf8(insert));
            state.emitting = false;
            const auto seen = openkey::Snapshot::read(ic->surroundingText(), true);
            if (seen && state.nativeExpected && *seen == *state.nativeExpected) {
                state.nativeAwaitingUpdate = false;
                state.nativeObserved = seen;
            } else if (seen && state.nativeExpected &&
                       (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
                       openkey::selectedUrlCompletion(*state.nativeExpected, *seen)) {
                state.nativeCompletion = seen;
                state.nativeAwaitingUpdate = false;
            }
            if (!boundary) event.filterAndAccept();
        }
        if (boundary) forget();
        else update(ic);
    }

    // Record what the client must report after an IM edit of the native word.
    void observeNativeEdit(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        const auto seen = openkey::Snapshot::read(ic->surroundingText(), true);
        if (seen && state.nativeExpected && *seen == *state.nativeExpected) {
            state.nativeAwaitingUpdate = false;
            state.nativeObserved = seen;
        } else if (seen && state.nativeExpected &&
                   (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
                   openkey::selectedUrlCompletion(*state.nativeExpected, *seen)) {
            state.nativeCompletion = seen;
            state.nativeAwaitingUpdate = false;
        }
    }

    // The client reports the committed tail with a collapsed caret: ordinary
    // checked caret edit, as in any field.
    void applyNativeClear(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        if (!state.nativeClear) return;
        traceClear(ic, "apply");
        const auto clear = std::move(*state.nativeClear);
        state.nativeClear.reset();
        const auto tail = static_cast<unsigned>(clear.stage.text.size() - clear.base.text.size());
        const auto start = clear.base.cursor - clear.removed;
        const auto insert = clear.stage.text.substr(clear.base.cursor);
        auto expected = clear.base;
        expected.text.replace(start, clear.removed, insert);
        expected.cursor = expected.anchor = start + static_cast<unsigned>(insert.size());
        state.placed = openkey::utf8(clear.after);
        state.nativeObserved = clear.stage;
        state.nativeExpected = std::move(expected);
        state.nativeAwaitingUpdate = true;
        state.nativeCompletion.reset();
        state.nativeKeepOnReset = true;
        state.plainCommit = true;
        state.emitting = true;
        ic->deleteSurroundingText(-static_cast<int>(clear.removed + tail), clear.removed + tail);
        ic->commitString(openkey::utf8(insert));
        state.emitting = false;
        observeNativeEdit(ic);
        diagnose(ic, "native:url-completion-edit", "verified-clear");
        update(ic);
    }

    // The first letter after an unreported edit landed: the update is the
    // baseline when the character before its caret is that letter, with a
    // plain caret or a selected URL completion after it. Only that letter is
    // ever edited later (every edit re-checks the placed suffix).
    bool adoptNativeBase(fcitx::InputContext* ic, const std::optional<openkey::Snapshot>& snapshot, char32_t letter) {
        auto& state = *ic->propertyFor(&factory_);
        if (!snapshot || snapshot->cursor == 0 || snapshot->cursor > snapshot->text.size() ||
            snapshot->text[snapshot->cursor - 1] != letter || state.placed != openkey::utf8(std::u32string(1, letter)))
            return false;
        const bool collapsed = snapshot->cursor == snapshot->anchor;
        const bool completion = (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
            snapshot->cursor < snapshot->anchor && snapshot->anchor == snapshot->text.size();
        if (!collapsed && !completion) return false;
        openkey::Snapshot base{snapshot->text.substr(0, collapsed ? snapshot->text.size() : snapshot->cursor),
                               snapshot->cursor, snapshot->cursor};
        if (collapsed) base = *snapshot;
        state.nativeExpected = base;
        state.nativeObserved = base;
        state.nativeCompletion.reset();
        if (completion) state.nativeCompletion = snapshot;
        state.nativeAwaitingUpdate = false;
        diagnose(ic, "native:stale-cache", "adopted-update");
        return true;
    }

    // `final`: a key or settings change needs the state now. The committed
    // tail is already in the document and `placed`/`nativeExpected` describe
    // it, so an unconfirmed clear simply becomes an ordinary awaited edit.
    void resolveNativeClear(fcitx::InputContext* ic, bool final) {
        auto& state = *ic->propertyFor(&factory_);
        if (!state.nativeClear) return;
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        if (snapshot && *snapshot == state.nativeClear->stage) {
            applyNativeClear(ic);
            return;
        }
        if (snapshot && (ic->capabilityFlags() & fcitx::CapabilityFlag::Url) &&
            openkey::selectedUrlCompletion(state.nativeClear->stage, *snapshot)) {
            // The intermediate text was autocompleted again: it stays the
            // word's document text; the next key repeats the same two steps.
            traceClear(ic, "reautocompleted");
            state.nativeClear.reset();
            state.nativeClearBlocked = true;
            state.nativeCompletion = snapshot;
            state.nativeAwaitingUpdate = false;
            diagnose(ic, "native:url-completion-raw", "clear-reautocompleted");
            return;
        }
        if (!final) return;
        traceClear(ic, "unconfirmed");
        state.nativeClear.reset();
        diagnose(ic, "native:url-completion-raw", "clear-unconfirmed");
    }

    // Diagnostics only. Lengths, offsets and relations to the confirmed typed
    // text (base), never text or keysyms of letters. Bounded per clear.
    void traceClear(fcitx::InputContext* ic, const char* what, const fcitx::Key* key = nullptr) {
        auto& state = *ic->propertyFor(&factory_);
        if (!*config_.diagnostics || !state.clearTrace || !state.clearTrace->left) return;
        auto& trace = *state.clearTrace;
        --trace.left;
        ++trace.events;
        const auto& base = trace.base;
        const auto& source = ic->surroundingText();
        const auto snapshot = openkey::Snapshot::read(source, true);
        const auto elapsed = fcitx::now(CLOCK_MONOTONIC) - trace.started;
        std::ostringstream line;
        line << "OpenKey Direct trace: event=" << what << " n=" << trace.events
             << " elapsed_us=" << elapsed << " pending=" << state.nativeClear.has_value()
             << " emitting=" << state.emitting << " valid=" << snapshot.has_value()
             << " base_len=" << base.text.size() << " base_cursor=" << base.cursor;
        if (snapshot) {
            const auto& text = snapshot->text;
            const bool hasBase = text.size() >= base.text.size() && text.compare(0, base.text.size(), base.text) == 0;
            const bool inBase = text.size() <= base.text.size() && base.text.compare(0, text.size(), text) == 0;
            line << " len=" << text.size() << " cursor=" << snapshot->cursor << " anchor=" << snapshot->anchor
                 << " equals_base=" << (*snapshot == base) << " text_equals_base=" << (text == base.text)
                 << " starts_with_base=" << hasBase << " prefix_of_base=" << inBase
                 << " equals_expected=" << (state.nativeExpected && *snapshot == *state.nativeExpected);
        }
        if (key) {
            // Class only. A keycode or keysym would reveal what was typed
            // (alpha.4 logged letter keycodes when NumLock was on).
            const bool backspace = key->sym() == FcitxKey_BackSpace;
            const bool printable = !backspace && fcitx::Key::keySymToUnicode(key->sym()) >= 0x20;
            line << " key=" << (backspace ? "backspace" : printable ? "printable" :
                                key->isModifier() ? "modifier" : "other");
        }
        FCITX_INFO() << line.str();
    }

    // Focus/context changed while the clear was in flight: never edit what may
    // now be another field. The committed tail is already where it was typed.
    void dropNativeClear(fcitx::InputContext* ic, const char* cause) {
        auto& state = *ic->propertyFor(&factory_);
        if (!state.nativeClear) return;
        state.nativeClear.reset();
        diagnose(ic, "native:url-completion-raw", cause);
    }

    // Keep the syllable exclusively in Fcitx's panel. In particular, never
    // expose incremental text to an append-only terminal's hidden textarea.
    // Boundaries stay native: forwardKey is unreliable on GNOME/Wayland.
    void commitOnlyKey(fcitx::InputContext* ic, fcitx::KeyEvent& event) {
        if (event.isRelease() || event.key().isModifier()) return;
        auto& composition = ic->propertyFor(&factory_)->composition;
        const auto key = event.key();
        if (shortcutKey(key)) {
            commit(ic);
            return;
        }
        if (key.sym() == FcitxKey_Escape && !composition.empty()) {
            composition.clear();
            update(ic);
            event.filterAndAccept();
            return;
        }
        // A full replay buffer cannot accept another Backspace event. Finish
        // it first so native Backspace edits this word, not earlier text.
        if (composition.full()) commit(ic);
        if (key.sym() == FcitxKey_BackSpace) {
            if (composition.backspace()) {
                update(ic);
                event.filterAndAccept();
            }
            return;
        }
        const auto symbol = fcitx::Key::keySymToUnicode(key.sym());
        if (symbol < 128 && composition.append(static_cast<char>(symbol))) {
            update(ic);
            event.filterAndAccept();
            return;
        }
        commit(ic);
    }

    fcitx::Instance* instance_;
    InputConfig config_;
    fcitx::SimpleInputContextPropertyFactory<State> factory_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> surroundingWatcher_;
    openkey::ApplicationMemory applicationMemory_;
    mutable ApplicationMemoryView memoryView_;
    fcitx::InputContext* memoryFocus_ = nullptr;
    bool restoringMemory_ = false;
    std::vector<std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>>> memoryWatchers_;
    std::optional<openkey::GnomeFocus> bridgeFocus_;
    uint64_t memoryEpoch_ = 0, memoryKeys_ = 0;
    openkey::GnomeFocusBridge gnomeBridge_;

    static bool gnomeContext(fcitx::InputContext* ic) {
        return ic && ic->program() == "gnome-shell" && ibusClient(ic);
    }
    void resetMemoryFocus() {
        gnomeBridge_.cancel(); bridgeFocus_.reset(); memoryFocus_ = nullptr; ++memoryEpoch_;
    }
    void resolveGnome(openkey::GnomeFocusBridge::Callback callback) {
        gnomeBridge_.resolve([this, callback](const auto& first) {
            if (!first) { callback({}); return; }
            gnomeBridge_.resolve([first, callback](const auto& second) {
                callback(second && *first == *second ? second : std::nullopt);
            });
        });
    }
    std::string memoryProgram(fcitx::InputContext* ic) const {
        if (openkey::ApplicationRules::identifiable(ic->program())) return ic->program();
        if (*config_.gnomeIdentity && gnomeContext(ic) && memoryFocus_ == ic && bridgeFocus_) return bridgeFocus_->key();
        return {};
    }
    void restoreApplication(fcitx::InputContext* ic) {
        const auto target = applicationMemory_.lookup(memoryProgram(ic));
        if (!target.empty() && target != instance_->inputMethod(ic) && methodInGroup(target)) {
            restoringMemory_ = true;
            instance_->setCurrentInputMethod(ic, target, true);
            restoringMemory_ = false;
        }
        if (target.empty()) rememberApplication(ic);
    }
    bool baseMemoryEligible(fcitx::InputContext* ic) const {
        return *config_.rememberApplication && ic && ic->hasFocus() &&
            !(ic->capabilityFlags() & fcitx::CapabilityFlags{fcitx::CapabilityFlag::PasswordOrSensitive, fcitx::CapabilityFlag::Disable});
    }

    bool memoryEligible(fcitx::InputContext* ic) const {
        return baseMemoryEligible(ic) && !memoryProgram(ic).empty();
    }
    bool methodInGroup(const std::string& method) const {
        const auto& items = instance_->inputMethodManager().currentGroup().inputMethodList();
        return std::any_of(items.begin(), items.end(), [&](const auto& item) { return item.name() == method; });
    }
    void rememberApplication(fcitx::InputContext* ic) {
        if (!memoryEligible(ic)) return;
        const auto method = instance_->inputMethod(ic);
        if (!applicationMemory_.remember(memoryProgram(ic), method)) return;
        ApplicationMemoryConfig saved;
        saved.entries.setValue(applicationMemory_.encode());
        if (!fcitx::safeSaveAsIni(saved, "conf/openkey-app-memory.conf"))
            FCITX_WARN() << "OpenKey could not save application input method memory";
    }
    void loadApplicationMemory() {
        ApplicationMemoryConfig saved;
        fcitx::readAsIni(saved, "conf/openkey-app-memory.conf");
        applicationMemory_.load(*saved.entries);
    }

    bool finishBoundary(fcitx::InputContext* ic, fcitx::KeyEvent& event) {
        const bool finishWord = *config_.quickStartConsonant || *config_.quickEndConsonant || *config_.restoreWrongSpelling;
        if ((!*config_.macrosEnabled && !finishWord) || event.isRelease() || shortcutKey(event.key()) ||
            (event.key().sym() != FcitxKey_space && !enterKey(event.key()))) return false;
        auto& state = *ic->propertyFor(&factory_);
        if (state.composition.empty() || !state.recovery.empty() || state.preeditDrifted) return false;
        openkey::MacroTable::Entries entries;
        if (*config_.macrosEnabled && !openkey::MacroTable::parse(*config_.macroTable, entries)) return false;
        const auto found = entries.find(state.composition.text());
        const auto replacement = found != entries.end() ? found->second :
            finishWord ? state.composition.finishedText() : state.composition.text();
        if (replacement == state.composition.text()) return false;
        unsigned remove = 0;
        const bool buffered = styleFor(ic) == InputStyle::Preedit || styleFor(ic) == InputStyle::CommitOnly ||
            (styleFor(ic) == InputStyle::Direct && !state.direct && !state.owned &&
             !state.plainCommit && !state.repairNativePrime && !state.nativePrime);
        if (!buffered) {
            // Macro replacement never uses predicted/stale echoes, selected autocomplete,
            // or suffix-only ownership. No confirmed whole snapshot means no expansion.
            if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
                appendOnlyClient(ic) || surroundingTextUnreliable(ic)) return false;
            const auto snapshot = openkey::Snapshot::read(ic->surroundingText());
            const auto* baseline = styleFor(ic) == InputStyle::NativeDirect ?
                (state.nativeExpected && !state.nativeAwaitingUpdate && !state.nativeRawWord ? &*state.nativeExpected : nullptr) :
                (!state.tracker.pending() ? state.tracker.confirmed() : nullptr);
            if (!snapshot || !baseline || !(*snapshot == *baseline)) return false;
            const auto placed = styleFor(ic) == InputStyle::NativeDirect ? state.placed : state.composition.text();
            const auto word = openkey::codepoints(placed);
            if (word.empty() || snapshot->cursor < word.size() ||
                snapshot->text.substr(snapshot->cursor - word.size(), word.size()) != word) return false;
            remove = static_cast<unsigned>(word.size());
        }
        state.composition.clear();
        clearDirect(state);
        update(ic);
        state.emitting = true;
        if (remove) ic->deleteSurroundingText(-static_cast<int>(remove), remove);
        ic->commitString(replacement);
        state.emitting = false;
        // Boundary stays native, including Enter used to send a chat message.
        return true;
    }

    openkey::ApplicationRules::Entries appRules_;
    InputStyle styleFor(fcitx::InputContext* ic) const {
        const auto found = appRules_.find(ic->program());
        if (found == appRules_.end()) return *config_.style;
        if (found->second == "Preedit") return InputStyle::Preedit;
        if (found->second == "Direct") return InputStyle::Direct;
        if (found->second == "CommitOnly") return InputStyle::CommitOnly;
        return InputStyle::NativeDirect;
    }
    openkey::Composition::Options compositionOptions() const {
        return {*config_.checkSpelling, *config_.useModernOrthography, *config_.quickTelex,
            *config_.freeMark, *config_.allowConsonantZFWJ, *config_.quickStartConsonant,
            *config_.quickEndConsonant, *config_.restoreWrongSpelling};
    }
    bool compositionConfigChanged(const InputConfig& previous) const {
        return *config_.terminalDeleteFix != *previous.terminalDeleteFix ||
            *config_.freeMark != *previous.freeMark ||
            *config_.allowConsonantZFWJ != *previous.allowConsonantZFWJ ||
            *config_.quickStartConsonant != *previous.quickStartConsonant ||
            *config_.quickEndConsonant != *previous.quickEndConsonant ||
            *config_.restoreWrongSpelling != *previous.restoreWrongSpelling ||
            *config_.applicationRules != *previous.applicationRules ||
            *config_.macrosEnabled != *previous.macrosEnabled ||
            *config_.macroTable != *previous.macroTable ||
            *config_.style != *previous.style ||
            *config_.checkSpelling != *previous.checkSpelling ||
            *config_.useModernOrthography != *previous.useModernOrthography ||
            *config_.quickTelex != *previous.quickTelex;
    }

    void diagnose(fcitx::InputContext* ic, const char* reason, const char* cause = "none") {
        if (!*config_.diagnostics) return;
        auto& state = *ic->propertyFor(&factory_);
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        auto comparison = state.tracker.compare(snapshot);
        const bool native = styleFor(ic) == InputStyle::NativeDirect;
        if (native) {
            comparison = {};
            comparison.reference = state.nativeExpected ? "native-expected" : "none";
            comparison.comparable = state.nativeExpected.has_value() && snapshot.has_value();
            if (comparison.comparable) {
                comparison.textChanged = state.nativeExpected->text != snapshot->text;
                comparison.cursorChanged = state.nativeExpected->cursor != snapshot->cursor;
                comparison.anchorChanged = state.nativeExpected->anchor != snapshot->anchor;
            }
        }
        const bool nativeCompletion = native && snapshot && state.nativeCompletion && *snapshot == *state.nativeCompletion;
        const char* snapshotStatus = native ?
            (!snapshot ? "invalid" : !state.nativeExpected ? "untracked" :
                nativeCompletion ? "url-completion" :
                *state.nativeExpected == *snapshot ? "expected" : "unexpected") : state.tracker.snapshotStatus(snapshot);
        const std::string signature = std::string(reason) + ":" + cause + ":" + comparison.reference +
            std::to_string(comparison.comparable) + std::to_string(comparison.textChanged) +
            std::to_string(comparison.cursorChanged) + std::to_string(comparison.anchorChanged) +
            (native ? std::to_string(state.nativeAwaitingUpdate) + std::to_string(state.nativeRawWord) : "");
        if (state.lastReason == signature) return;
        state.lastReason = signature;
        // Only identifiers, enum classifications and boolean/capability state. Never log text,
        // keystrokes, URLs, selections, offsets or recovery contents.
        FCITX_INFO() << "OpenKey Direct: reason=" << reason << " app=" << ic->program()
                     << " frontend=" << ic->frontend() << " capabilities="
                     << ic->capabilityFlags().toInteger() << " cause=" << cause
                     << " snapshot=" << snapshotStatus
                     << " pending=" << state.tracker.pending()
                     << " notified=" << state.pendingNotified
                     << " reference=" << comparison.reference << " comparable=" << comparison.comparable
                     << " text_changed=" << comparison.textChanged
                     << " cursor_changed=" << comparison.cursorChanged
                     << " anchor_changed=" << comparison.anchorChanged
                     << " ack_requires_update=" << state.tracker.requiresFreshUpdate()
                     << " native_waiting_update=" << (native && state.nativeAwaitingUpdate)
                     << " native_raw_word=" << (native && state.nativeRawWord)
                     << " native_url_completion=" << nativeCompletion;
    }

    static void clearDirect(State& state) {
        if (state.timeout) state.timeout->setEnabled(false);
        state.nativeClear.reset();
        state.nativeAdopt = 0;
        state.tracker.clear();
        state.queue.clear();
        state.direct = state.bypass = false;
        state.pendingNotified = false;
        state.primed = false;
        state.nativePrime = false;
        state.owned = false;
        state.retainOwned = false;
        state.ownedLen = 0;
        state.ownedSkip = 0;
        state.forcePreedit = false;
        state.plainCommit = false;
        state.ibusAfterEnter = false;
        state.ibusNativeWord = false;
        state.ibusPrimeExpected.reset();
        state.nativeExpected.reset();
        state.nativeObserved.reset();
        state.nativeCompletion.reset();
        state.nativeRawWord = false;
        state.nativeKeepOnReset = false;
        state.nativeResetHeld = false;
        state.nativeAwaitingUpdate = false;
        state.placed.clear();
        state.terminalWord.clear();
        state.preeditDrifted = false;
        state.repairNativePrime = false;
        state.nativePrimeChar = 0;
        state.nativePrimeBase.reset();
        state.previousWord.reset();
        state.trailingSpaces.clear();
    }

    // Replay original queued keys rather than rewriting any text after loss of
    // sync. This is forwarding client events, not synthesizing Backspace keys.
    void settle(fcitx::InputContext* ic, bool forceRecovery = false) {
        auto& state = *ic->propertyFor(&factory_);
        auto queued = std::move(state.queue);
        state.queue.clear();
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) return;
        if (forceRecovery || (state.direct && state.tracker.observe(
                openkey::Snapshot::read(ic->surroundingText(), true)) == openkey::DirectInput::Observation::Changed)) {
            // Original keys no longer have a safe destination. Do not replay
            // them over a new selection/cursor: retain printable text for an
            // explicit Enter/Escape choice instead of silently losing it.
            for (const auto& item : queued) {
                const auto modifiers = fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                    fcitx::KeyState::Super, fcitx::KeyState::Super2, fcitx::KeyState::Meta};
                if (item.release || (item.key.states() & modifiers)) continue;
                const auto ch = fcitx::Key::keySymToUnicode(item.key.sym());
                if (item.key.sym() == FcitxKey_BackSpace && !state.recovery.empty())
                    popCharacter(state.recovery);
                else if (printable(ch)) state.recovery += fcitx::utf8::UCS4ToUTF8(ch);
            }
            return;
        }
        const bool emitting = state.emitting;
        state.emitting = true;
        for (const auto& item : queued) ic->forwardKey(item.key, item.release, item.time);
        state.emitting = emitting;
    }

    void abandon(fcitx::InputContext* ic, const char* cause) {
        auto& state = *ic->propertyFor(&factory_);
        // Inspect before settle()/clearDirect() change the pending state.
        diagnose(ic, "edit-aborted", cause);
        settle(ic);
        clearDirect(state);
        state.composition.clear();
        state.bypass = true; // Do not interpret an untracked suffix as a new word.
        diagnose(ic, state.recovery.empty() ? "bypass:unconfirmed-edit" : "recovery:context-changed");
        update(ic);
    }

    void finishContexts() {
        instance_->inputContextManager().foreach([this](fcitx::InputContext* ic) {
            auto& state = *ic->propertyFor(&factory_);
            if (!state.active) return true;
            // Same field, new settings: finish or release the clear first.
            if (state.nativeClear) resolveNativeClear(ic, true);
            if (state.composition.empty() && !state.direct && !state.bypass && state.queue.empty()) {
                if (state.nativeExpected || state.nativeRawWord) clearDirect(state);
                state.composition.setOptions(compositionOptions());
                return true; // Never clear the panel owned by another input method.
            }
            if (state.plainCommit) state.composition.clear(); // already in the document
            else if (state.direct) {
                settle(ic, state.bypass);
                state.composition.clear();
            } else commit(ic);
            state.recovery.clear();
            clearDirect(state);
            state.composition.setOptions(compositionOptions());
            update(ic);
            return true;
        });
    }

    // Replay only composing keys. Releases/modifiers for those keys are dropped:
    // the corresponding press already mutated the document via commitString.
    void flushQueue(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        auto queued = std::move(state.queue);
        state.queue.clear();
        while (!queued.empty() && !state.tracker.pending()) {
            auto item = queued.front();
            queued.pop_front();
            if (item.release || item.key.isModifier()) continue;
            fcitx::KeyEvent event(ic, item.key, false, item.time);
            fcitx::InputMethodEntry unused("openkey", "OpenKey", "vi", "openkey");
            keyEvent(unused, event);
            if (!event.accepted()) {
                // Last resort for a composing key that slipped through. Wayland
                // may still drop this; prefer commitString paths above.
                ic->forwardKey(item.key, false, item.time);
            }
            if (!state.direct) {
                state.queue = std::move(queued);
                settle(ic, state.bypass);
                if (!state.recovery.empty()) update(ic);
                return;
            }
        }
        state.queue.insert(state.queue.end(), queued.begin(), queued.end());
    }

    void reconcile(fcitx::InputContext* ic, bool fromUpdate = false) {
        auto& state = *ic->propertyFor(&factory_);
        const bool pending = state.tracker.pending();
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        const auto status = state.tracker.observe(snapshot, fromUpdate);
        if (status == openkey::DirectInput::Observation::Changed) {
            if (!pending && state.queue.empty()) {
                diagnose(ic, "context-invalidated", snapshot ? "surrounding-mismatch" : "surrounding-invalid");
                // Focus/selection/completion changes while idle end ownership
                // of the old suffix, not Vietnamese input for the whole field.
                clearDirect(state);
                state.composition.clear();
                diagnose(ic, "context-reset");
                update(ic);
            } else abandon(ic, snapshot ? "surrounding-mismatch" : "surrounding-invalid");
            return;
        }
        if (status != openkey::DirectInput::Observation::Ready) {
            if (!pending) diagnose(ic, "context-wait", "late-known-echo");
            return;
        }
        // The native first key landed. Later timeouts must not replay it as preedit.
        state.nativePrime = false;
        if (state.timeout) state.timeout->setEnabled(false);
        flushQueue(ic);
    }

    // End Direct ownership of the current word. Caller must leave the key
    // unaccepted so the compositor/app still receive Enter/nav/shortcut.
    void endDirectWord(fcitx::InputContext* ic, const fcitx::Key& key) {
        auto& state = *ic->propertyFor(&factory_);
        state.composition.clear();
        state.previousWord.reset();
        state.trailingSpaces.clear();
        diagnose(ic, enterKey(key) ? "boundary:enter-passthrough" :
            (shortcutKey(key) ? "boundary:shortcut-passthrough" : "boundary:nav-passthrough"));
        clearDirect(state);
    }

    DirectOutcome directKey(fcitx::InputContext* ic, const fcitx::Key& key, bool textKey, uint32_t symbol) {
        auto& state = *ic->propertyFor(&factory_);
        const bool delimiter = !textKey && printable(symbol);
        if ((!textKey && !delimiter && key.sym() != FcitxKey_BackSpace) || shortcutKey(key))
            return DirectOutcome::Ignored;
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) ||
            state.tracker.observe(snapshot) != openkey::DirectInput::Observation::Ready) {
            if (!state.tracker.pending() && state.queue.empty()) {
                diagnose(ic, "context-invalidated", !(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "capability-withdrawn" : (snapshot ? "key-snapshot-not-current" : "surrounding-invalid"));
                clearDirect(state);
                state.composition.clear();
                if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) || !snapshot)
                    return DirectOutcome::Ignored;
                state.direct = true;
                state.tracker.observe(snapshot);
                diagnose(ic, "context-reset");
            } else {
                abandon(ic, !(ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "capability-withdrawn" : (snapshot ? "surrounding-mismatch" : "surrounding-invalid"));
                return DirectOutcome::Ignored;
            }
        }
        diagnose(ic, snapshot && snapshot->cursor != snapshot->anchor ? "direct:selection-start" : "direct:ready");
        if (state.composition.full()) {
            state.composition.clear();
            state.previousWord.reset();
            state.trailingSpaces.clear();
            state.tracker.clear();
            state.tracker.observe(openkey::Snapshot::read(ic->surroundingText(), true));
        }
        auto candidate = state.composition;
        const bool reopen = key.sym() == FcitxKey_BackSpace && candidate.empty() &&
            state.previousWord && !state.trailingSpaces.empty();
        std::string before = state.composition.text();
        std::string after;
        bool handled;
        if (reopen) {
            // Validate the ENTIRE retained word plus its spaces, not just one
            // blank or a matching word somewhere else in the document.
            before = state.previousWord->text() + state.trailingSpaces;
            after = before.substr(0, before.size() - 1); // ASCII Space only.
            if (state.trailingSpaces.size() == 1) candidate = *state.previousWord;
            handled = true;
        } else {
            handled = delimiter || (textKey ? candidate.append(static_cast<char>(symbol)) : candidate.backspace());
            after = delimiter ? before + fcitx::utf8::UCS4ToUTF8(symbol) : candidate.text();
        }
        if (!handled) { clearDirect(state); return DirectOutcome::Ignored; }
        // First letter after focus with a collapsed caret: let the client insert
        // natively (unaccepted). LibreOffice uses Preedit fallback; Qt often
        // ignores the first commitString but still accepts a real key. prepare()
        // records the expected surrounding and stays pending until the client
        // notifies; never emit commitString and never accept+forwardKey (alpha.13).
        if (!state.primed && textKey && !delimiter && !reopen && before.empty() &&
            snapshot && snapshot->cursor == snapshot->anchor) {
            if (!state.tracker.prepare(before, after)) {
                abandon(ic, "prepare-rejected");
                return DirectOutcome::Ignored;
            }
            state.composition = std::move(candidate);
            state.previousWord.reset();
            state.trailingSpaces.clear();
            state.primed = true;
            state.pendingNotified = false;
            state.nativePrime = true;
            const auto primed = openkey::codepoints(state.composition.text());
            state.nativePrimeChar = primed.empty() ? 0 : primed.front();
            armAckTimeout(ic);
            diagnose(ic, "direct:prime-native");
            return DirectOutcome::Ignored;
        }
        const auto edit = state.tracker.prepare(before, after);
        if (!edit) { abandon(ic, "prepare-rejected"); return DirectOutcome::Ignored; }
        if (reopen) {
            state.trailingSpaces.pop_back();
            if (state.trailingSpaces.empty()) {
                state.previousWord.reset();
                diagnose(ic, "boundary-restored");
            }
        } else if (delimiter) {
            if (symbol == ' ' && !state.composition.empty()) {
                state.previousWord = state.composition;
                state.trailingSpaces = " ";
            } else if (symbol == ' ' && state.previousWord && state.trailingSpaces.size() < 32) {
                state.trailingSpaces.push_back(' ');
            } else {
                state.previousWord.reset();
                state.trailingSpaces.clear();
            }
            candidate.clear();
        } else {
            // Starting the next word or deleting inside it ends this bounded
            // one-word boundary history. Never scan/reconstruct arbitrary text.
            state.previousWord.reset();
            state.trailingSpaces.clear();
        }
        state.composition = std::move(candidate);
        // Set pending before emitting: some frontends synchronously update
        // surrounding text from commit/delete callbacks.
        state.pendingNotified = false;
        state.primed = true;
        state.emitting = true;
        if (edit->removed) ic->deleteSurroundingText(-static_cast<int>(edit->removed), edit->removed);
        if (!edit->inserted.empty()) ic->commitString(edit->inserted);
        state.emitting = false;
        armAckTimeout(ic);
        // Synchronous frontend notifications are observed by the watcher even
        // while emitting; polling the cache without such an event is not ACK.
        reconcile(ic, state.pendingNotified);
        return DirectOutcome::Consumed;
    }

    // Replace only the span we previously commitString'd. Does not consult
    // surrounding text, so a chat box that never ACKs still shows the word.
    void commitOwned(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        auto chars = openkey::codepoints(state.composition.text());
        if (state.ownedSkip && chars.size() >= state.ownedSkip)
            chars.erase(chars.begin(), chars.begin() + state.ownedSkip);
        else if (state.ownedSkip) chars.clear();
        const auto text = openkey::utf8(chars);
        const auto count = static_cast<unsigned>(chars.size());
        state.emitting = true;
        if (state.ownedLen)
            ic->deleteSurroundingText(-static_cast<int>(state.ownedLen), state.ownedLen);
        if (!text.empty()) ic->commitString(text);
        state.emitting = false;
        state.ownedLen = count;
    }

    // The native first key is still unACKed and another letter arrived. Commit
    // the engine word immediately. If surrounding text proves the native key
    // landed, remove that one character first so it is not doubled.
    bool takeOwnership(fcitx::InputContext* ic, const fcitx::Key& key, uint32_t symbol) {
        auto& state = *ic->propertyFor(&factory_);
        if (key.sym() == FcitxKey_BackSpace) {
            if (!state.composition.backspace()) return false;
        } else if (!letterOrDigit(symbol) || !state.composition.append(static_cast<char>(symbol))) {
            return false;
        }
        const auto chars = openkey::codepoints(state.composition.text());
        // Wayland already delivered the unaccepted first key. Keep committing
        // only the suffix while that character is still the start of the word
        // (tieeps → tiếp). If the engine replaced it (dd → đ), commit the
        // whole word so the chat is not left with the raw key.
        const bool prefixKept = !chars.empty() && chars.front() == state.nativePrimeChar;
        state.tracker.clear();
        state.queue.clear();
        state.nativePrime = false;
        state.direct = true;
        state.owned = true;
        state.ownedLen = 0;
        state.ownedSkip = prefixKept ? 1u : 0u;
        if (state.timeout) state.timeout->setEnabled(false);
        commitOwned(ic);
        return true;
    }

    void armAckTimeout(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        if (!state.tracker.pending()) return;
        if (!state.timeout) {
            state.timeout = instance_->eventLoop().addTimeEvent(CLOCK_MONOTONIC,
                fcitx::now(CLOCK_MONOTONIC) + 250000, 0,
                [this, weak = ic->watch()](fcitx::EventSourceTime* source, uint64_t) {
                    source->setEnabled(false);
                    if (auto* context = weak.get()) {
                        auto& pendingState = *context->propertyFor(&factory_);
                        if (!pendingState.tracker.pending()) return true;
                        // Unaccepted first key never confirmed. LibreOffice/Qt on
                        // the GNOME ibus bridge drop that key and do not ACK, so
                        // forwarding the queue loses the word. Show it as preedit
                        // instead. Enter/Home stay on the unaccepted path.
                        if (pendingState.nativePrime) {
                            diagnose(context, "edit-aborted", "ack-timeout");
                            promoteNativePrime(context);
                        } else abandon(context, "ack-timeout");
                    }
                    return true;
                });
        }
        state.timeout->setTime(fcitx::now(CLOCK_MONOTONIC) + 250000);
        state.timeout->setOneShot();
    }

    // First Direct key was not ACKed. Fold the queued letters into the
    // composition and keep this context on Preedit. Does not forwardKey and
    // does not commitString; Enter/Home remain unaccepted later.
    void promoteNativePrime(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        auto queued = std::move(state.queue);
        state.queue.clear();
        const auto base = state.tracker.confirmed()
            ? std::optional<openkey::Snapshot>(*state.tracker.confirmed()) : std::nullopt;
        const auto primedChar = state.nativePrimeChar;
        for (const auto& item : queued) {
            if (item.release || item.key.isModifier() || shortcutKey(item.key)) continue;
            const auto symbol = fcitx::Key::keySymToUnicode(item.key.sym());
            if (item.key.sym() == FcitxKey_BackSpace) {
                state.composition.backspace();
                continue;
            }
            if (!letterOrDigit(symbol) || !state.composition.append(static_cast<char>(symbol)))
                break;
        }
        clearDirect(state);
        state.forcePreedit = true;
        if (primedChar && base) {
            state.repairNativePrime = true;
            state.nativePrimeChar = primedChar;
            state.nativePrimeBase = base;
        }
        update(ic);
    }

    void update(fcitx::InputContext* ic) {
        const auto& state = *ic->propertyFor(&factory_);
        const auto value = (state.direct || state.plainCommit || styleFor(ic) == InputStyle::NativeDirect)
            ? std::string{} : state.composition.text();
        fcitx::Text text(value);
        text.setCursor(static_cast<int>(value.size()));
        ic->inputPanel().reset();
        if (!state.recovery.empty()) {
            ic->inputPanel().setAuxUp(fcitx::Text("Phím chưa nhập / Pending: " + state.recovery));
            ic->inputPanel().setAuxDown(fcitx::Text("Bỏ chọn vùng / place caret; Enter: chèn / insert; Escape: hủy / cancel"));
        }
        // Append-only clients (xterm.js) get the word only from commitString;
        // the syllable is shown in the server panel while typing.
        if (state.hiddenPreedit || styleFor(ic) == InputStyle::CommitOnly) {
            ic->inputPanel().setClientPreedit(fcitx::Text{});
            ic->inputPanel().setPreedit(text);
        } else {
            if ((ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit) || state.forcePreedit)
                ic->inputPanel().setClientPreedit(text);
            if (!(ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit) || state.forcePreedit)
                ic->inputPanel().setPreedit(text);
        }
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    void commit(fcitx::InputContext* ic) {
        auto& state = *ic->propertyFor(&factory_);
        if (state.repairNativePrime && state.nativePrimeChar && state.nativePrimeBase) {
            const auto snap = openkey::Snapshot::read(ic->surroundingText());
            // The unaccepted key is in the document when surrounding text proves
            // it, and also when the cache never moved (Cursor ACK lags the key).
            const bool removeNative = snap && (nativePrimeLanded(*state.nativePrimeBase,
                state.nativePrimeChar, *snap) || *snap == *state.nativePrimeBase);
            if (removeNative) {
                state.emitting = true;
                ic->deleteSurroundingText(-1, 1);
                state.emitting = false;
            }
            state.repairNativePrime = false;
            state.nativePrimeChar = 0;
            state.nativePrimeBase.reset();
        }
        auto& composition = state.composition;
        const auto text = composition.text();
        composition.clear();
        // A promoted syllable is over. The next word follows InputStyle again,
        // so Direct does not stay underlined for the rest of the field.
        if (styleFor(ic) == InputStyle::Direct) state.forcePreedit = false;
        update(ic);
        if (!text.empty() &&
            !(ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive))
            ic->commitString(text);
    }
};

class OpenKeyFactory : public fcitx::AddonFactory {
    fcitx::AddonInstance* create(fcitx::AddonManager* manager) override {
        return new OpenKeyAddon(manager->instance());
    }
};
} // namespace

FCITX_ADDON_FACTORY(OpenKeyFactory)
