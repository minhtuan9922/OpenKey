#define WIN32_LEAN_AND_MEAN
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/TrayPopup.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace OpenKeyTrayPopup;
static int passed = 0;
static void Check(bool condition, char const* name)
{ if (!condition) { std::fprintf(stderr, "FAIL %s\n", name); std::exit(1); } ++passed; }
static VARIANT Child(LONG id) { VARIANT value{}; value.vt = VT_I4; value.lVal = id; return value; }

namespace OpenKeyTrayPopup
{
    struct TestAccess
    {
        static void Fixture(bool dark, UINT dpi, char const* preview)
        {
            Popup popup(dark, dpi);
            auto& entries = *popup.m_snapshot;
            entries = {
                { L"Bật Tiếng Việt", 900, MFS_CHECKED, false, {} },
                { L"", 0, 0, true, {} },
                { L"Bảng điều khiển...", 1000, MFS_DEFAULT, false, {} },
                { L"Không khả dụng", 11, MFS_DISABLED, false, {} },
                { L"Giao diện", 0, 0, false, {{ L"Theo hệ thống", 1020, 0, false, {} }} },
                { L"Thoát", 2000, 0, false, {} }
            };
            LOGFONTW font{}; font.lfHeight = -popup.Px(16); font.lfWeight = FW_NORMAL;
            wcscpy_s(font.lfFaceName, L"Segoe UI"); popup.m_font = CreateFontIndirectW(&font);
            font.lfWeight = FW_BOLD; popup.m_bold = CreateFontIndirectW(&font);
            Check(popup.m_font && popup.m_bold, "fixture fonts");
            auto panel = std::unique_ptr<Popup::Panel>(new Popup::Panel);
            panel->owner = &popup; panel->data = std::make_shared<AccessibleData>();
            panel->data->snapshot = popup.m_snapshot; panel->data->entries = &entries;
            panel->width = popup.Px(300); panel->height = popup.Px(195);
            panel->bounds = { 0,0,panel->width,panel->height };
            panel->viewport = { 0,popup.Px(10),panel->width,panel->height - popup.Px(10) };
            int y = 0;
            for (auto const& entry : entries)
            { const int height = popup.Px(entry.separator ? 9 : 32); panel->rows.push_back({0,y,panel->width,y + height}); y += height; }
            panel->total = y;
            popup.m_panels.push_back(std::move(panel));
            auto& p = *popup.m_panels.front(); popup.Locations(p);
            popup.Key(VK_DOWN); Check(p.data->selected == 0, "down first enabled");
            popup.Key(VK_DOWN); Check(p.data->selected == 2, "skip separator");
            popup.Key(VK_DOWN); Check(p.data->selected == 4, "skip disabled");
            popup.Key(VK_END); Check(p.data->selected == 5, "end last");
            popup.Key(VK_RIGHT); Check(!popup.m_done, "right on leaf does not execute");
            popup.Key(VK_DOWN); Check(p.data->selected == 0, "down wraps");
            popup.Key(VK_UP); Check(p.data->selected == 5, "up wraps");
            popup.Key(VK_HOME); Check(p.data->selected == 0, "home first");
            popup.Character(L'g'); Check(p.data->selected == 4, "first letter selects submenu");
            popup.Key(VK_PRIOR); Check(p.data->selected >= 0 && entries[p.data->selected].Enabled(), "page up enabled");
            popup.Scroll(10000); Check(p.scroll == 0, "short menu does not scroll");
            const LONG viewportBottom = p.viewport.bottom;
            p.viewport.bottom = popup.Px(100);
            popup.Scroll(10000); Check(p.scroll > 0 && p.scroll == p.total - (p.viewport.bottom - p.viewport.top), "scroll clamps bottom");
            popup.Scroll(-10000); Check(p.scroll == 0, "scroll clamps top");
            p.viewport.bottom = viewportBottom; popup.Locations(p);
            Check(popup.Hit(p, { popup.Px(50), popup.Px(15) }) == 0, "hit first row");
            Check(popup.Hit(p, { popup.Px(50), 0 }) == -1, "padding noninteractive");
            popup.AccessibleSelect(&p, -1, true); Check(!popup.m_done, "invalid accessible action rejected");

            popup.Select(0, 0);
            p.data->locations[0].left = -123;
            popup.Select(0, 0); Check(p.data->locations[0].left == -123, "unchanged selection avoids rebuilding locations");
            popup.Locations(p);
            popup.Hover(0, 4, 1000); Check(popup.m_hoverPending && p.data->selected == 4, "submenu hover selects but delays opening");
            popup.Hover(0, 4, 1100); Check(popup.m_hoverDue == 1200, "same row movement does not restart hover deadline");
            popup.FlushHover(1199); Check(popup.m_panels.size() == 1 && popup.m_hoverPending, "submenu not opened before deadline");
            popup.Hover(0, 0, 1150); Check(!popup.m_hoverPending && p.data->selected == 0, "leaf hover cancels stale submenu");
            popup.FlushHover(2000); Check(!popup.m_done && popup.m_panels.size() == 1, "canceled hover never opens or invokes command");
            popup.Select(0, 4);
            auto child = std::unique_ptr<Popup::Panel>(new Popup::Panel);
            child->owner = &popup; child->data = std::make_shared<AccessibleData>();
            child->data->snapshot = popup.m_snapshot; child->data->entries = &entries[4].children;
            child->rows.push_back({0,0,p.width,popup.Px(32)}); child->viewport = p.viewport;
            popup.m_panels.push_back(std::move(child));
            popup.Hover(0, 5, 3000);
            Check(popup.m_panels.size() == 2 && p.data->selected == 4, "crossing parent row keeps submenu alive");
            popup.Hover(1, 0, 3050);
            Check(!popup.m_hoverPending && popup.m_panels.size() == 2, "entering submenu cancels pending parent switch");
            popup.Hover(0, 5, 4000); popup.FlushHover(4199);
            Check(popup.m_panels.size() == 2, "parent switch waits for deadline");
            popup.FlushHover(4200);
            Check(popup.m_panels.size() == 1 && p.data->selected == 5 && !popup.m_done, "stable hover closes old child without invoking leaf");
            popup.Hover(0, 4, 5000); popup.Key(VK_HOME);
            Check(!popup.m_hoverPending && p.data->selected == 0, "keyboard cancels mouse delay and acts immediately");

            // Paint to a memory bitmap under the same rounded clip as the window.
            HDC dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = p.width; info.bmiHeader.biHeight = -p.height;
            info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
            void* pixels = nullptr; HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            Check(dc && bitmap, "memory bitmap"); const auto old = SelectObject(dc, bitmap);
            RECT full{ 0,0,p.width,p.height }; SetDCBrushColor(dc, RGB(255,0,255)); FillRect(dc, &full, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            HRGN rounded = CreateRoundRectRgn(0, 0, p.width + 1, p.height + 1, popup.Px(24), popup.Px(24));
            SelectClipRgn(dc, rounded);
            p.data->selected = 2; popup.BufferedPaint(p, dc, full);
            Check(p.buffer && p.bitmap, "back buffer created");
            const auto cachedBuffer = p.buffer; const auto cachedBitmap = p.bitmap;
            const DWORD gdiCount = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
            for (int frame = 0; frame < 300; ++frame) popup.BufferedPaint(p, dc, full);
            Check(p.buffer == cachedBuffer && p.bitmap == cachedBitmap, "repeated paint reuses back buffer");
            Check(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) == gdiCount, "repeated paint does not allocate GDI objects");
            SelectClipRgn(dc, nullptr); // GetPixel cannot read outside the selected clip.
            Check(GetPixel(dc, 0, 0) == RGB(255,0,255), "rounded corner transparent to desktop");
            Check(GetPixel(dc, p.width / 2, popup.Px(4)) == OpenKeyTrayTheme::Colors(dark).background, "top padding theme");
            Check(GetPixel(dc, popup.Px(8), popup.Px(68)) == OpenKeyTrayTheme::Colors(dark).hover, "rounded hover");
            const int arrowY = popup.Px(10) + (p.rows[4].top + p.rows[4].bottom) / 2;
            Check(GetPixel(dc, p.width - popup.Px(18) - popup.Px(3), arrowY - popup.Px(4)) == OpenKeyTrayTheme::Colors(dark).text, "submenu arrow theme color");
            if (preview)
            {
                BITMAPFILEHEADER file{}; file.bfType = 0x4d42; file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
                file.bfSize = file.bfOffBits + p.width * p.height * 4;
                std::ofstream output(preview, std::ios::binary);
                output.write(reinterpret_cast<char const*>(&file), sizeof(file));
                output.write(reinterpret_cast<char const*>(&info.bmiHeader), sizeof(info.bmiHeader));
                GdiFlush(); output.write(static_cast<char const*>(pixels), static_cast<std::streamsize>(p.width) * p.height * 4);
                Check(output.good(), "write offscreen preview");
            }
            SelectClipRgn(dc, nullptr); DeleteObject(rounded); SelectObject(dc, old); DeleteObject(bitmap); DeleteDC(dc);
            popup.Select(0, 5); popup.Key(VK_RETURN); Check(popup.m_done && popup.m_command == 2000, "enter exact command");
            popup.m_done = false; popup.Key(VK_ESCAPE); Check(popup.m_done, "escape cancel");
        }
    };
}
int main(int argc, char** argv)
{
    HMENU menu = CreatePopupMenu(), child = CreatePopupMenu();
    AppendMenuW(child, MF_CHECKED, 1020, L"Theo hệ thống");
    AppendMenuW(menu, MF_CHECKED, 900, L"Bật Tiếng Việt");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_GRAYED, 11, L"Disabled");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(child), L"Giao diện");
    SetMenuDefaultItem(menu, 900, FALSE);
    auto snapshot = std::make_shared<std::vector<Entry>>();
    Check(Snapshot(menu, *snapshot) && snapshot->size() == 4, "snapshot count");
    Check((*snapshot)[0].id == 900 && ((*snapshot)[0].state & MFS_CHECKED) && ((*snapshot)[0].state & MFS_DEFAULT), "snapshot command/check/default");
    Check((*snapshot)[1].separator && !(*snapshot)[1].Enabled() && !(*snapshot)[2].Enabled(), "snapshot separator/disabled");
    Check((*snapshot)[3].children.size() == 1 && (*snapshot)[3].children[0].id == 1020, "snapshot submenu");
    Check(GetMenuDefaultItem(menu, FALSE, 0) == 900, "native menu unmodified");
    DestroyMenu(menu);
    Check(!Snapshot(nullptr, *snapshot), "invalid menu fallback");
    Check(Next(*snapshot, 0, 1) == 3 && Next(*snapshot, 3, 1) == 0, "enabled navigation wraps");
    Check(Next({}, -1, 1) == -1, "empty navigation");
    Check(Name(L"&Mở && đóng") == L"Mở & đóng", "accessible label strips mnemonic");
    RECT work{ -100,0,600,500 };
    auto fit = Fit({590,490},300,250,work);
    Check(fit.left == 300 && fit.top == 250 && fit.right == 600 && fit.bottom == 500, "fit right bottom");
    fit = Fit({-300,-200},800,700,work);
    Check(EqualRect(&fit, &work), "oversize fit negative monitor");
    Check(!ContainsRounded({0,0,300,200},{0,0},12) && ContainsRounded({0,0,300,200},{150,0},12), "rounded hit testing");

    auto data = std::make_shared<AccessibleData>(); data->snapshot = snapshot; data->entries = snapshot.get();
    Accessible* provider = new Accessible(data);
    LONG count = 0; Check(provider->get_accChildCount(&count) == S_OK && count == 4, "MSAA child count");
    BSTR name = nullptr; Check(provider->get_accName(Child(1), &name) == S_OK && std::wstring(name) == L"Bật Tiếng Việt", "MSAA Unicode name"); SysFreeString(name);
    VARIANT value{};
    Check(provider->get_accRole(Child(0), &value) == S_OK && value.lVal == ROLE_SYSTEM_MENUPOPUP, "MSAA popup role");
    Check(provider->get_accRole(Child(2), &value) == S_OK && value.lVal == ROLE_SYSTEM_SEPARATOR, "MSAA separator role");
    Check(provider->get_accState(Child(1), &value) == S_OK && (value.lVal & STATE_SYSTEM_CHECKED) && (value.lVal & STATE_SYSTEM_INVISIBLE), "MSAA checked and destroyed state");
    Check(provider->get_accState(Child(3), &value) == S_OK && (value.lVal & STATE_SYSTEM_UNAVAILABLE), "MSAA disabled state");
    Check(provider->get_accState(Child(4), &value) == S_OK && (value.lVal & STATE_SYSTEM_HASPOPUP), "MSAA submenu state");
    Check(provider->get_accName(Child(9), &name) == E_INVALIDARG, "MSAA invalid child");
    Check(provider->accNavigate(NAVDIR_FIRSTCHILD, Child(0), &value) == S_OK && value.lVal == 1, "MSAA first child");
    Check(provider->accNavigate(NAVDIR_NEXT, Child(4), &value) == S_FALSE, "MSAA navigation edge");
    Check(provider->accDoDefaultAction(Child(1)) == S_FALSE && provider->accDoDefaultAction(Child(3)) == E_INVALIDARG, "MSAA no action after destroy or disabled");
    provider->Release();
    // USER/GDI creates process-local stock objects lazily on the first paint.
    // Warm that path once, then require every subsequent panel lifetime to balance.
    TestAccess::Fixture(false, 96, nullptr);
    for (UINT dpi : {96u,144u,192u}) for (bool dark : {false,true})
    {
        const DWORD gdiCount = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        TestAccess::Fixture(dark,dpi, dpi == 96 && argc >= 3 ? argv[dark ? 1 : 2] : nullptr);
        const DWORD after = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        Check(after == gdiCount, "panel destruction releases buffer and fonts");
    }
    std::printf("%d custom tray popup checks passed; model/MSAA/offscreen only, no window, registry, engine or hook.\n", passed);
}
