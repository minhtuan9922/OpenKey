#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretSessionRestore.h"
#include <iostream>
#include <stdexcept>
using namespace OpenKeyCaretSession;
namespace {
    int checks = 0;
    void Check(bool result, const char* label) { if (!result) throw std::runtime_error(label); ++checks; }
    struct Backend {
        bool desktop = true, succeeds = true;
        ProfileState state{true, false, true, false};
        int reads = 0, restores = 0;
        bool OnUserDesktop() { return desktop; }
        ProfileState Read() { ++reads; return state; }
        bool Restore() { ++restores; return succeeds; }
    };
    const ProfileState vie{true, true, true, false}, eng{true, false, true, false};
    struct Storage {
        OpenKeyCaretSettings::Choice value = OpenKeyCaretSettings::Choice::Unknown;
        bool fail = false; int writes = 0;
        OpenKeyCaretSettings::Choice Read() { return value; }
        bool Save(bool selected) { ++writes; if (fail) return false; value = selected ? OpenKeyCaretSettings::Choice::Selected : OpenKeyCaretSettings::Choice::Inactive; return true; }
    };
    void RestartPersistence() {
        using OpenKeyCaretSettings::RememberedChoice;
        using OpenKeyCaretSettings::Choice;
        Storage storage; RememberedChoice first;
        Check(!first.Load(storage) && !storage.writes, "new installations do not enable Caret Editing or write a default");
        Check(first.Observe(true, storage) && storage.value == Choice::Selected, "VIE is saved immediately without waiting for shutdown");
        Check(first.Observe(true, storage) && storage.writes == 1, "duplicate selection checkpoints avoid duplicate writes");
        RememberedChoice nextLogon; Recovery recovery; Backend backend;
        recovery.Startup(nextLogon.Load(storage));
        backend.desktop = false;
        for (int i = 0; i < 12; ++i) Check(recovery.Tick(backend) && !backend.reads && !backend.restores, "startup waits for the user desktop beyond the normal unlock timeout");
        backend.desktop = true;
        Check(!recovery.Tick(backend) && backend.restores == 1, "a new process restores saved VIE after logon when the desktop is ready");
        SelectionMemory selection; const auto editor = reinterpret_cast<HWND>(1);
        selection.Observe(editor, true, true); selection.Lock();
        if (selection.Observe(editor, false, true)) nextLogon.Observe(false, storage);
        Check(storage.value == Choice::Selected, "shutdown teardown cannot overwrite persisted VIE after end-session begins");
        selection.Unlock();
        if (selection.Observe(editor, false, true)) nextLogon.Observe(false, storage);
        Check(storage.value == Choice::Inactive, "canceled logout accepts a later explicit ENG selection");
        RememberedChoice later; recovery.Startup(later.Load(storage)); const int restored = backend.restores;
        Check(!recovery.Tick(backend) && backend.restores == restored, "saved ENG does not automatically enable VIE on restart");
        storage.value = Choice::Selected; // Another process, e.g. Overview.
        Check(later.Observe(false, storage) && storage.value == Choice::Inactive, "UI writes in another process cannot leave a stale persistence cache");
        storage.fail = true;
        Check(!later.Observe(true, storage) && storage.value == Choice::Inactive, "write failures preserve the last saved choice");
        storage.fail = false;
        Check(later.Observe(true, storage) && storage.value == Choice::Selected, "failed writes can be retried by a later checkpoint");
        recovery.Startup(true); backend.state.enabled = false;
        Check(!recovery.Tick(backend) && backend.restores == restored, "startup never enables a disabled TSF profile");
        recovery.Startup(true); recovery.Cancel(); backend.state.enabled = true;
        Check(!recovery.Tick(backend) && backend.restores == restored, "an explicit choice cancels pending startup restoration");
    }
    void Cycles() {
        Recovery recovery; Backend backend;
        Check(!recovery.Unlock() && !recovery.Tick(backend) && !backend.reads, "unlock without lock never switches the user's language");
        for (int i = 0; i < 100; ++i) {
            recovery.Lock(vie);
            Check(!recovery.Tick(backend), "no retry runs while locked");
            recovery.Lock(eng);
            Check(recovery.Unlock(), "duplicate lock cannot discard the original VIE snapshot");
            Check(!recovery.Tick(backend) && backend.restores == i + 1, "unlock restores VIE once after fallback to ENG");
            Check(!recovery.Unlock() && !recovery.Tick(backend) && backend.restores == i + 1, "duplicate unlock never restarts a completed restore");
            recovery.Lock(eng);
            Check(!recovery.Unlock() && !recovery.Tick(backend) && backend.restores == i + 1, "ENG selected before locking remains ENG");
        }
    }
    void StartMenuTransitions() {
        using namespace OpenKeyCaretSettings;
        const auto editor = reinterpret_cast<HWND>(1), start = reinterpret_cast<HWND>(2);
        Check(ShellSurface(L"Windows.UI.Core.CoreWindow", L"StartMenuExperienceHost.exe"), "Start host is a transient shell surface");
        Check(ShellSurface(L"XamlExplorerHostIslandWindow", L"SearchHost.exe"), "Search host is transient too");
        Check(ShellSurface(L"Shell_TrayWnd", L"explorer.exe"), "taskbar flyout transition is not an editor choice");
        Check(!ShellSurface(L"CabinetWClass", L"explorer.exe") && !ShellSurface(L"Chrome_WidgetWin_1", L"Code.exe"), "regular Explorer and editor windows are not blanket-excluded");
        for (UINT win : {VK_LWIN, VK_RWIN}) for (bool original : {false, true}) {
            Storage storage; RememberedChoice remembered; SelectionMemory selection;
            remembered.Load(storage);
            selection.Observe(editor, original, true); remembered.Observe(original, storage);
            const auto expected = storage.value; const int writes = storage.writes;
            selection.Key(win, true, false);
            if (selection.Observe(editor, false, true)) remembered.Observe(false, storage);
            Check(storage.value == expected && storage.writes == writes, "Win key shields the old editor before Start gains foreground");
            selection.Key(win, false, false);
            if (selection.Observe(editor, false, true)) remembered.Observe(false, storage);
            Check(storage.value == expected, "releasing Win before the Start foreground event does not save transient ENG");
            selection.Foreground(start, true, true);
            selection.Key('A', true, true); // Typing in Start search is still shell input.
            if (selection.Observe(start, !original, true, true)) remembered.Observe(!original, storage);
            Check(storage.value == expected && storage.writes == writes, "Start/Search cannot overwrite either saved VIE or saved ENG");
            const auto locked = selection.Lock();
            Check(locked.known && locked.ours == original, "shutdown or Win+L retains the pre-Start selection snapshot");
            selection.Unlock();
            RememberedChoice afterRestart; Recovery recovery; Backend backend;
            recovery.Startup(afterRestart.Load(storage)); recovery.Tick(backend);
            Check(backend.restores == (original ? 1 : 0), "startup follows the saved editor choice rather than Start's temporary layout");
        }
        SelectionMemory selection; Storage storage; RememberedChoice remembered; remembered.Load(storage);
        selection.Observe(editor, true, true); remembered.Observe(true, storage);
        selection.Key(VK_LWIN, true, false); selection.Key(VK_SPACE, true, false);
        Check(selection.Observe(editor, false, true) && remembered.Observe(false, storage) && storage.value == Choice::Inactive,
            "Win+Space remains an explicit ENG choice and is persisted");
        selection.Foreground(start, true, true);
        Check(selection.Observe(start, false, true, true), "Win+Space choice survives focus moving into the shell language picker");
        selection.Key(VK_LWIN, false, false); selection.Foreground(editor, true);
        selection.Key('A', true, false);
        Check(selection.Observe(editor, true, true), "normal editor selection resumes after the shell gesture ends");
        selection.Foreground(start, true, true);
        selection.Key(VK_RWIN, true, true); selection.Key(VK_SPACE, true, true);
        Check(selection.Observe(start, false, true, true), "an explicit Win+Space choice is accepted even from a shell surface");
        selection.Key(VK_RWIN, false, true); selection.Foreground(editor, true);
        Check(selection.Observe(editor, true, true), "returning to the editor ends shell suppression");
        selection.Key(VK_LWIN, true, false); selection.Key('L', true, false);
        Check(!selection.Observe(editor, false, true) && selection.Lock().ours, "Win+L preserves VIE before lock notification arrives");
        selection.Unlock();
        Check(!selection.WatchingKeys() && selection.Observe(editor, false, true), "unlock clears stale Win modifier state if keyup was missed");
        SelectionMemory mouse; mouse.Observe(editor, true, true); mouse.Foreground(start, true, true);
        Check(!mouse.Observe(start, false, true, true) && mouse.Lock().ours, "mouse-opened Start preserves the editor snapshot too");
    }
    void BeforeLockSelection() {
        Check(!ForegroundState(true, 0).known, "no foreground layout waits instead of activating too early");
        for (LANGID language : {LANGID(0x0409), LANGID(0x0809), OpenKeyTextService::Language}) {
            const auto state = ForegroundState(true, language);
            Check(state.known && state.enabled && !state.ours && !state.otherService,
                "foreground VI/English cannot prove restoration succeeded from a stale background thread profile");
        }
        Check(ForegroundState(true, 0x0804).otherService, "another foreground input language is preserved");
        Check(!ForegroundState(false, 0x0409).enabled, "disabled profile remains disabled");
        Check(OpenKeyCaretSelection::Message(true) && OpenKeyCaretSelection::Message(false) &&
            OpenKeyCaretSelection::Message(true) != OpenKeyCaretSelection::Message(false), "selected and deselected messages remain distinct");
        const auto editor = reinterpret_cast<HWND>(1), other = reinterpret_cast<HWND>(2);
        SelectionMemory selection; Recovery recovery; Backend backend;
        selection.Observe(editor, true, true);
        selection.Observe(editor, false, false);
        selection.Foreground(other, false);
        recovery.Lock(selection.Lock());
        Check(recovery.Unlock() && !recovery.Tick(backend) && backend.restores == 1,
            "VIE captured on the real desktop survives secure-desktop ENG before WTS lock");
        selection.Observe(editor, false, true);
        Check(selection.Lock().ours, "profile callbacks delivered while locked cannot overwrite the snapshot");
        selection.Unlock();
        selection.Observe(editor, false, true);
        Check(!selection.Lock().ours, "explicit ENG chosen on the user desktop is remembered");
        selection.Unlock(); selection.Observe(editor, true, true);
        selection.Foreground(editor, true);
        Check(selection.Lock().ours, "late foreground notification for the same editor preserves its checkpoint");
        selection.Unlock(); selection.Foreground(other, true);
        Check(!selection.Lock().known, "a different app without a TIP checkpoint cannot inherit stale VIE from the previous editor");
        selection.Unlock(); selection.Observe(other, true, true);
        Check(selection.Lock().ours, "new editor supplies its own selected-profile checkpoint");
        selection.Reset();
        Check(!selection.Lock().known, "shutdown clears the desktop selection checkpoint");
    }
    void Boundaries() {
        for (ProfileState initial : {ProfileState{}, ProfileState{true, true, false, false}, eng}) {
            Recovery recovery; Backend backend; recovery.Lock(initial);
            Check(!recovery.Unlock() && !recovery.Tick(backend) && !backend.restores, "unknown/disabled/ENG snapshot does not restore");
        }
        for (ProfileState current : {vie, ProfileState{true, false, false, false}, ProfileState{true, false, true, true}}) {
            Recovery recovery; Backend backend; backend.state = current;
            recovery.Lock(vie); recovery.Unlock();
            Check(!recovery.Tick(backend) && !backend.restores, "already VIE, disabled profile or another chosen IME stops recovery");
        }
        Recovery recovery; Backend backend; recovery.Lock(vie); recovery.Unlock();
        backend.desktop = false;
        Check(recovery.Tick(backend) && !backend.reads && !backend.restores, "secure desktop never queries or activates a profile");
        backend.desktop = true; backend.state = {};
        Check(recovery.Tick(backend) && !backend.restores, "temporary TSF read failure waits for a later timer");
        backend.state = eng; backend.succeeds = false;
        Check(recovery.Tick(backend) && backend.restores == 1, "temporary activation failure is retryable");
        backend.succeeds = true;
        Check(!recovery.Tick(backend) && backend.restores == 2, "successful recovery stops further retries");
        for (int mode : {0, 1, 2}) {
            Recovery pending; Backend busy;
            busy.desktop = mode != 0; busy.state.known = mode != 1; busy.succeeds = false;
            pending.Lock(vie); pending.Unlock();
            for (int i = 0; i < 20; ++i) pending.Tick(busy);
            Check(!pending.Pending() && busy.restores <= 10 && busy.reads <= 10, "retries are bounded even if desktop or TSF never becomes ready");
        }
        recovery.Lock(vie); recovery.Unlock(); recovery.Cancel();
        Check(!recovery.Pending() && !recovery.Tick(backend), "shutdown/logoff cancels pending activation");
        recovery.Lock(vie); recovery.Unlock(); recovery.Lock(eng);
        Check(!recovery.Unlock() && !recovery.Tick(backend), "new lock replaces a pending previous cycle");
    }
}
int main() {
    try { Cycles(); RestartPersistence(); StartMenuTransitions(); BeforeLockSelection(); Boundaries(); std::cout << checks << " caret session recovery checks passed (fixtures only; no lock or profile changes).\n"; return 0; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
