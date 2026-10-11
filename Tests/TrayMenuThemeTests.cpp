#define WIN32_LEAN_AND_MEAN
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/TrayMenuTheme.h"
#include <cstdio>
#include <cstdlib>

using namespace OpenKeyTrayTheme;
static int passed = 0;
static void Check(bool condition, char const* name)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); }
    ++passed;
}
static MENUITEMINFOW Info(HMENU menu, UINT position)
{
    MENUITEMINFOW value{}; value.cbSize = sizeof(value);
    value.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STATE | MIIM_SUBMENU | MIIM_ID;
    Check(GetMenuItemInfoW(menu, position, TRUE, &value) != FALSE, "get item");
    return value;
}
static void Fixture(bool dark, UINT dpi, bool checkResources = true)
{
    HMENU menu = CreatePopupMenu(), submenu = CreatePopupMenu();
    Check(menu && submenu, "allocate native menus without displaying them");
    AppendMenuW(menu, MF_STRING | MF_CHECKED, 10, L"&Vietnamese input");
    AppendMenuW(menu, MF_STRING | MF_GRAYED, 11, L"Disabled");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(submenu), L"Theme");
    AppendMenuW(menu, MF_STRING, 12, L"Text expansion");
    AppendMenuW(menu, MF_STRING, 13, L"Text conversion");
    AppendMenuW(submenu, MF_STRING, 20, L"System");
    AppendMenuW(submenu, MF_STRING | MF_CHECKED, 21, L"Light");
    AppendMenuW(submenu, MF_STRING, 22, L"Dark");
    SetMenuDefaultItem(menu, 10, FALSE);
    MENUITEMINFOW original{}; original.cbSize = sizeof(original);
    original.fMask = MIIM_DATA; original.dwItemData = 123;
    SetMenuItemInfoW(menu, 0, TRUE, &original);
    const auto before = Info(menu, 0);
    MENUINFO background{}; background.cbSize = sizeof(background); background.fMask = MIM_BACKGROUND;
    GetMenuInfo(menu, &background);
    const auto originalBackground = background.hbrBack;
    const DWORD gdiBefore = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    {
        Session session(menu, dark, dpi);
        Check(session.Ready(), "attach themed session");
        auto value = Info(menu, 0);
        Check((value.fType & MFT_OWNERDRAW) != 0 && value.dwItemData != 123, "owner draw data attached");
        Check(value.wID == 10 && value.fState == before.fState, "command checked/default state preserved");
        Check(GetMenuDefaultItem(menu, FALSE, 0) == 10, "default command retained");
        const auto* accessible = reinterpret_cast<MSAAMENUINFO*>(value.dwItemData);
        Check(accessible->dwMSAASignature == MSAA_MENU_SIG && accessible->cchWText == 17 &&
            std::wstring(accessible->pszWText) == L"&Vietnamese input", "MSAA name exposed");
        Check((Info(submenu, 1).fType & MFT_OWNERDRAW) && (Info(submenu, 1).fState & MFS_CHECKED), "submenu checked state");
        Check(Info(menu, 3).hSubMenu == submenu, "submenu handle unchanged");
        MEASUREITEMSTRUCT measure{}; measure.CtlType = ODT_MENU; measure.itemData = value.dwItemData;
        Check(session.Measure(&measure) && measure.itemWidth > static_cast<UINT>(MulDiv(100, dpi, 96)) &&
            measure.itemHeight >= static_cast<UINT>(MulDiv(28, dpi, 96)), "text and DPI dimensions");
        auto separator = Info(menu, 2);
        MEASUREITEMSTRUCT spacing{}; spacing.CtlType = ODT_MENU; spacing.itemData = separator.dwItemData;
        Check(session.Measure(&spacing) && spacing.itemHeight == static_cast<UINT>(MulDiv(9, dpi, 96)), "separator DPI height");
        Check(!session.Measure(nullptr), "null measure rejected");
        measure.itemData = 456; Check(!session.Measure(&measure), "foreign item rejected");
        Check(HIWORD(session.MenuChar(L'v', menu)) == MNC_EXECUTE && LOWORD(session.MenuChar(L'v', menu)) == 0, "explicit mnemonic");
        Check(HIWORD(session.MenuChar(L'd', menu)) == MNC_IGNORE, "disabled command excluded");
        Check(HIWORD(session.MenuChar(L'x', menu)) == MNC_IGNORE, "unknown mnemonic");
        Check(HIWORD(session.MenuChar(L't', menu)) == MNC_SELECT && LOWORD(session.MenuChar(L't', menu)) == 3, "first letter selects not executes");
        MENUITEMINFOW highlighted{}; highlighted.cbSize = sizeof(highlighted);
        highlighted.fMask = MIIM_STATE; highlighted.fState = MFS_HILITE;
        Check(SetMenuItemInfoW(menu, 3, TRUE, &highlighted) != FALSE, "fixture selected submenu");
        Check(LOWORD(session.MenuChar(L't', menu)) == 4, "duplicate initial cycles forward");
        highlighted.fState = 0; SetMenuItemInfoW(menu, 3, TRUE, &highlighted);
        highlighted.fState = MFS_HILITE; SetMenuItemInfoW(menu, 5, TRUE, &highlighted);
        Check(LOWORD(session.MenuChar(L't', menu)) == 3, "duplicate initial wraps");
        highlighted.fState = 0; SetMenuItemInfoW(menu, 5, TRUE, &highlighted);

        HDC dc = CreateCompatibleDC(nullptr);
        BITMAPINFO bitmap{}; bitmap.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap.bmiHeader.biWidth = 700; bitmap.bmiHeader.biHeight = -100;
        bitmap.bmiHeader.biPlanes = 1; bitmap.bmiHeader.biBitCount = 32;
        void* pixels = nullptr;
        HBITMAP surface = CreateDIBSection(dc, &bitmap, DIB_RGB_COLORS, &pixels, nullptr, 0);
        Check(dc && surface, "offscreen surface");
        const auto oldBitmap = SelectObject(dc, surface);
        const auto oldFont = GetCurrentObject(dc, OBJ_FONT);
        const int oldMode = GetBkMode(dc);
        const auto oldText = GetTextColor(dc);
        DRAWITEMSTRUCT draw{}; draw.CtlType = ODT_MENU; draw.hDC = dc;
        draw.itemData = value.dwItemData; draw.rcItem = { 0, 0, 700, 100 };
        draw.itemState = ODS_CHECKED | ODS_DEFAULT;
        Check(session.Draw(&draw) && GetPixel(dc, 0, 0) == Colors(dark).background, "normal background");
        Check(GetCurrentObject(dc, OBJ_FONT) == oldFont && GetBkMode(dc) == oldMode && GetTextColor(dc) == oldText, "GDI state restored");
        draw.itemState = ODS_SELECTED;
        Check(session.Draw(&draw) && GetPixel(dc, 0, 0) == Colors(dark).hover, "hover background");
        draw.itemState = ODS_DISABLED | ODS_SELECTED;
        Check(session.Draw(&draw) && GetPixel(dc, 0, 0) == Colors(dark).background, "disabled no hover");
        draw.itemData = separator.dwItemData;
        Check(session.Draw(&draw) && GetPixel(dc, MulDiv(15, dpi, 96), 50) == Colors(dark).separator, "separator paint");
        Check(!session.Draw(nullptr), "null draw rejected");
        SelectObject(dc, oldBitmap); DeleteObject(surface); DeleteDC(dc);
        GetMenuInfo(menu, &background);
        Check(background.hbrBack != originalBackground, "menu gutter uses themed brush");
        session.Restore(); session.Restore();
        Check(!session.Ready(), "restore idempotent");
    }
    const auto after = Info(menu, 0);
    Check(after.fType == before.fType && after.dwItemData == 123, "original type/data restored");
    GetMenuInfo(menu, &background);
    Check(background.hbrBack == originalBackground, "original gutter restored");
    Check(!(Info(submenu, 1).fType & MFT_OWNERDRAW), "submenu restored");
    wchar_t text[80]{}; GetMenuStringW(menu, 0, text, 80, MF_BYPOSITION);
    Check(std::wstring(text) == L"&Vietnamese input", "native string retained");
    const DWORD gdiAfter = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    if (checkResources) Check(gdiAfter == gdiBefore, "no GDI object leak after system cache warmup");
    {
        Session native(menu, dark, dpi, true);
        Check(!native.Ready() && !(Info(menu, 0).fType & MFT_OWNERDRAW), "High Contrast native fallback");
    }
    DestroyMenu(menu); // Includes submenu.
}
int main()
{
    // Initialize USER32/GDI's shared stock objects before measuring ownership.
    HDC warmup = CreateCompatibleDC(nullptr);
    RECT warmupText{}; DrawTextW(warmup, L"warmup", -1, &warmupText, DT_CALCRECT);
    SelectObject(warmup, GetStockObject(DC_PEN));
    SelectObject(warmup, GetStockObject(DC_BRUSH));
    SetDCPenColor(warmup, RGB(0,0,0)); SetDCBrushColor(warmup, RGB(255,255,255));
    DeleteDC(warmup);
    using OpenKeyUITheme::Theme;
    Check(!ResolveDark(Theme::Light, true), "explicit light");
    Check(ResolveDark(Theme::Dark, false), "explicit dark");
    Check(ResolveDark(Theme::System, true) && !ResolveDark(Theme::System, false), "system follows OS");
    // USER32 lazily caches its system menu fonts on first metrics/paint use.
    // Exercise that path once, then require exact stability for every replay.
    Fixture(false, 96, false);
    for (UINT dpi : { 96u, 144u, 192u }) for (bool dark : { false, true }) Fixture(dark, dpi);
    std::printf("%d tray theme checks passed; offscreen only, no popup, registry or engine activation.\n", passed);
}
