// Minimal disposable guest editor. All keyboard input comes from the human/UI test.
#include <windows.h>
#include <string>
static HWND editor;
static void Save() {
    std::wstring text(GetWindowTextLengthW(editor) + 1, L'\0');
    const auto count = GetWindowTextW(editor, text.data(), static_cast<int>(text.size()));
    HANDLE file = CreateFileW(L"C:\\OpenKeySandboxLogs\\typing.txt", GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written; const WORD bom = 0xfeff;
        WriteFile(file, &bom, 2, &written, nullptr);
        WriteFile(file, text.data(), count * sizeof(wchar_t), &written, nullptr);
        CloseHandle(file);
    }
}
static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        editor = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL,
            0, 0, 0, 0, window, nullptr, GetModuleHandleW(nullptr), nullptr);
        SendMessageW(editor, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
        SetFocus(editor); return 0;
    case WM_SIZE: MoveWindow(editor, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE); return 0;
    case WM_COMMAND: if (LOWORD(wparam) == 1) { Save(); return 0; } break;
    case WM_CLOSE: Save(); DestroyWindow(window); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(window, message, wparam, lparam);
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    wchar_t user[256]; DWORD size = 256;
    if (!GetUserNameW(user, &size) || wcscmp(user, L"WDAGUtilityAccount") ||
        GetFileAttributesW(L"C:\\OpenKeySandboxLogs") == INVALID_FILE_ATTRIBUTES) return 2;
    WNDCLASSW cls{}; cls.lpfnWndProc = WindowProc; cls.hInstance = instance; cls.lpszClassName = L"OpenKeySandboxTypingEditor";
    cls.hCursor = LoadCursorW(nullptr, IDC_IBEAM); RegisterClassW(&cls);
    HMENU menu = CreateMenu(); AppendMenuW(menu, MF_STRING, 1, L"Save (Ctrl+S)");
    HWND window = CreateWindowW(cls.lpszClassName, L"OpenKey Sandbox Typing Test", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 450, nullptr, menu, instance, nullptr);
    ShowWindow(window, show); UpdateWindow(window);
    ACCEL key{FCONTROL | FVIRTKEY, 'S', 1}; HACCEL accelerator = CreateAcceleratorTableW(&key, 1);
    MSG message;
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!TranslateAcceleratorW(window, accelerator, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    return 0;
}
