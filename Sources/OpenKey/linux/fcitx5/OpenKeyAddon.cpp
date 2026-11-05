#include "Composition.h"
#include "DirectInput.h"
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
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>

namespace {
FCITX_CONFIG_ENUM(InputStyle, Preedit, Direct);
FCITX_CONFIGURATION(InputConfig,
    fcitx::Option<InputStyle> style{this, "InputStyle", "Chế độ gõ / Input style", InputStyle::Preedit};
    fcitx::Option<bool> diagnostics{this, "DirectDiagnostics", "Direct diagnostics (no text content)", false};
    fcitx::Option<bool> quickTelex{this, "QuickTelex", "Gõ nhanh Telex / Quick Telex", false};
    fcitx::Option<bool> checkSpelling{this, "CheckSpelling", "Kiểm tra chính tả / Check spelling", true};
    fcitx::Option<bool> useModernOrthography{this, "UseModernOrthography", "Đặt dấu kiểu mới / Modern tone placement", true};
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

class OpenKeyAddon : public fcitx::InputMethodEngine {
public:
    explicit OpenKeyAddon(fcitx::Instance* instance) : instance_(instance) {
        instance->inputContextManager().registerProperty("openkeyState", &factory_);
        fcitx::readAsIni(config_, "conf/openkey.conf");
        surroundingWatcher_ = instance->watchEvent(fcitx::EventType::InputContextSurroundingTextUpdated,
            fcitx::EventWatcherPhase::InputMethod, [this](fcitx::Event& event) {
                auto* ic = static_cast<fcitx::InputContextEvent&>(event).inputContext();
                auto& state = *ic->propertyFor(&factory_);
                if (!state.active) return;
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
    void setConfig(const fcitx::RawConfig& raw) override {
        const auto previous = config_;
        config_.load(raw, true);
        if (compositionConfigChanged(previous)) finishContexts();
        // Keep future/third-party keys when applying a partial settings update.
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
        if (compositionConfigChanged(previous)) finishContexts();
    }

    void activate(const fcitx::InputMethodEntry& entry, fcitx::InputContextEvent& event) override {
        auto* state = event.inputContext()->propertyFor(&factory_);
        state->active = true;
        state->lastReason.clear();
        // EN/VI comes back through activate. A recovery buffer left by the
        // previous method would swallow every following key.
        state->recovery.clear();
        clearDirect(*state);
        state->composition = openkey::Composition(entry.uniqueName() == "openkey-vni" ? vVNI : vTelex,
                                                  compositionOptions());
        auto* ic = event.inputContext();
        // Some clients (Chrome address bar, Qt) ignore the first IM edit until a
        // preedit/focus handshake. Send an empty client preedit when Direct.
        if (*config_.style == InputStyle::Direct &&
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
        auto& composition = state.composition;
        if (ic->capabilityFlags() & fcitx::CapabilityFlag::PasswordOrSensitive) {
            diagnose(ic, "bypass:sensitive-field");
            clearDirect(state);
            state.recovery.clear();
            composition.clear();
            update(ic);
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
        const bool terminalPreedit = ibusClient(ic) && *config_.style == InputStyle::Direct &&
            directRewriteUnsafe(ic) && (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit);
        state.hiddenPreedit = ibusClient(ic) && *config_.style == InputStyle::Direct && appendOnlyClient(ic);
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
        if (ibusClient(ic) && *config_.style == InputStyle::Direct && !terminalPreedit) {
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
            *config_.style == InputStyle::Direct &&
            !surroundingTextUnreliable(ic) &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText) &&
            openkey::Snapshot::read(ic->surroundingText(), true)) state.direct = true;
        else if (!state.direct && *config_.style == InputStyle::Direct && surroundingTextUnreliable(ic) &&
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
        if (*config_.style == InputStyle::Direct && !state.forcePreedit && !surroundingTextUnreliable(ic) &&
            !terminalPreedit)
            diagnose(ic, !composition.empty() ? "fallback:preedit-word-in-progress" :
                ((ic->capabilityFlags() & fcitx::CapabilityFlag::SurroundingText)
                    ? "fallback:invalid-surrounding-text" : "fallback:no-surrounding-capability"));
        // Direct requested but the client can neither surround-edit nor show
        // preedit (e.g. many terminals): do not steal keys into an invisible
        // server composition. Preedit style still uses server-side preedit.
        // A client that CAN show preedit must still convert (chees → chế).
        if (*config_.style == InputStyle::Direct && !state.forcePreedit &&
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
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        settle(ic);
        state.recovery.clear();
        clearDirect(state);
        state.composition.clear();
        update(ic);
    }

    void deactivate(const fcitx::InputMethodEntry&, fcitx::InputContextEvent& event) override {
        auto* ic = event.inputContext();
        auto& state = *ic->propertyFor(&factory_);
        const bool sensitive = static_cast<bool>(ic->capabilityFlags() &
            fcitx::CapabilityFlag::PasswordOrSensitive);
        // Focus-out with client preedit: the frontend commits that preedit.
        // Committing again duplicates the word.
        // plainCommit text is already in the document. Committing it again duplicates.
        const bool clientHoldsPreedit = event.type() == fcitx::EventType::InputContextFocusOut &&
            (ic->capabilityFlags() & fcitx::CapabilityFlag::Preedit) && !state.direct &&
            !state.plainCommit && !state.hiddenPreedit;
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
    fcitx::Instance* instance_;
    InputConfig config_;
    fcitx::SimpleInputContextPropertyFactory<State> factory_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> surroundingWatcher_;

    openkey::Composition::Options compositionOptions() const {
        return {*config_.checkSpelling, *config_.useModernOrthography, *config_.quickTelex};
    }
    bool compositionConfigChanged(const InputConfig& previous) const {
        return *config_.style != *previous.style ||
            *config_.checkSpelling != *previous.checkSpelling ||
            *config_.useModernOrthography != *previous.useModernOrthography ||
            *config_.quickTelex != *previous.quickTelex;
    }

    void diagnose(fcitx::InputContext* ic, const char* reason, const char* cause = "none") {
        if (!*config_.diagnostics) return;
        auto& state = *ic->propertyFor(&factory_);
        const auto snapshot = openkey::Snapshot::read(ic->surroundingText(), true);
        const auto comparison = state.tracker.compare(snapshot);
        const std::string signature = std::string(reason) + ":" + cause + ":" + comparison.reference +
            std::to_string(comparison.comparable) + std::to_string(comparison.textChanged) +
            std::to_string(comparison.cursorChanged) + std::to_string(comparison.anchorChanged);
        if (state.lastReason == signature) return;
        state.lastReason = signature;
        // Only identifiers, enum classifications and boolean/capability state. Never log text,
        // keystrokes, URLs, selections, offsets or recovery contents.
        FCITX_INFO() << "OpenKey Direct: reason=" << reason << " app=" << ic->program()
                     << " frontend=" << ic->frontend() << " capabilities="
                     << ic->capabilityFlags().toInteger() << " cause=" << cause
                     << " snapshot=" << state.tracker.snapshotStatus(snapshot)
                     << " pending=" << state.tracker.pending()
                     << " notified=" << state.pendingNotified
                     << " reference=" << comparison.reference << " comparable=" << comparison.comparable
                     << " text_changed=" << comparison.textChanged
                     << " cursor_changed=" << comparison.cursorChanged
                     << " anchor_changed=" << comparison.anchorChanged
                     << " ack_requires_update=" << state.tracker.requiresFreshUpdate();
    }

    static void clearDirect(State& state) {
        if (state.timeout) state.timeout->setEnabled(false);
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
        state.placed.clear();
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
            if (state.composition.empty() && !state.direct && !state.bypass && state.queue.empty()) {
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
        const auto value = (state.direct || state.plainCommit) ? std::string{} : state.composition.text();
        fcitx::Text text(value);
        text.setCursor(static_cast<int>(value.size()));
        ic->inputPanel().reset();
        if (!state.recovery.empty()) {
            ic->inputPanel().setAuxUp(fcitx::Text("Phím chưa nhập / Pending: " + state.recovery));
            ic->inputPanel().setAuxDown(fcitx::Text("Bỏ chọn vùng / place caret; Enter: chèn / insert; Escape: hủy / cancel"));
        }
        // Append-only clients (xterm.js) get the word only from commitString;
        // the syllable is shown in the server panel while typing.
        if (state.hiddenPreedit) {
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
        if (*config_.style == InputStyle::Direct) state.forcePreedit = false;
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
