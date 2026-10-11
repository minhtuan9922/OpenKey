#pragma once

// Documented Win32 owner-draw only: no private uxtheme ordinals or global
// process theme overrides. A session owns item data until the menu loop ends.
#include <windows.h>
#include <oleacc.h>
#include <shellscalingapi.h>
#include <roapi.h>
#include <windows.ui.viewmanagement.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>
#include <memory>
#include <string>
#include <vector>
#include <cwctype>
#include "../OpenKey.UI/UiThemeSettings.h"

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")

namespace OpenKeyTrayTheme
{
    struct Palette { COLORREF background, text, disabled, hover, separator; };
    inline Palette Colors(bool dark)
    {
        return dark ? Palette{ RGB(32,32,32), RGB(245,245,245), RGB(160,160,160), RGB(58,58,58), RGB(70,70,70) }
                    : Palette{ RGB(249,249,249), RGB(24,24,24), RGB(110,110,110), RGB(230,230,230), RGB(215,215,215) };
    }
    inline bool ResolveDark(OpenKeyUITheme::Theme theme, bool systemDark)
    {
        return theme == OpenKeyUITheme::Theme::Dark ||
            (theme == OpenKeyUITheme::Theme::System && systemDark);
    }
    inline bool SystemDark()
    {
        // The same documented foreground test used by Fluent, without adding
        // another registry preference. Balance only successful RoInitialize.
        const HRESULT initialized = RoInitialize(RO_INIT_SINGLETHREADED);
        bool dark = false;
        {
            Microsoft::WRL::ComPtr<IInspectable> instance;
            Microsoft::WRL::ComPtr<ABI::Windows::UI::ViewManagement::IUISettings3> settings;
            Microsoft::WRL::Wrappers::HStringReference name(RuntimeClass_Windows_UI_ViewManagement_UISettings);
            if (SUCCEEDED(RoActivateInstance(name.Get(), &instance)) &&
                SUCCEEDED(instance.As(&settings)))
            {
                ABI::Windows::UI::Color foreground{};
                if (SUCCEEDED(settings->GetColorValue(
                    ABI::Windows::UI::ViewManagement::UIColorType_Foreground, &foreground)))
                    dark = OpenKeyUITheme::IsColorLight(foreground.R, foreground.G, foreground.B);
            }
        }
        if (SUCCEEDED(initialized)) RoUninitialize();
        return dark;
    }
    inline bool HighContrast()
    {
        HIGHCONTRASTW value{};
        value.cbSize = sizeof(value);
        // If querying accessibility fails, prefer the native menu as well.
        return !SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(value), &value, 0) ||
            (value.dwFlags & HCF_HIGHCONTRASTON) != 0;
    }
    inline UINT CursorDpi(POINT point)
    {
        UINT x = 96, y = 96;
        if (FAILED(GetDpiForMonitor(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), MDT_EFFECTIVE_DPI, &x, &y)))
            return 96;
        return x ? x : 96;
    }

    class Session
    {
        struct Item
        {
            MSAAMENUINFO accessible{}; // Must be first: names exposed to MSAA.
            std::wstring text;
            HMENU menu = nullptr;
            UINT position = 0, originalType = 0;
            ULONG_PTR originalData = 0;
        };
        struct Menu { HMENU handle; HBRUSH originalBackground; };
        std::vector<std::unique_ptr<Item>> m_items;
        std::vector<Menu> m_menus;
        Palette m_colors;
        UINT m_dpi;
        HFONT m_font = nullptr, m_bold = nullptr;
        HBRUSH m_background = nullptr;
        bool m_ready = false;

        int Px(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
        Item const* Find(ULONG_PTR data) const
        {
            for (auto const& item : m_items)
                if (reinterpret_cast<ULONG_PTR>(item.get()) == data) return item.get();
            return nullptr;
        }
        bool Attach(HMENU menu)
        {
            MENUINFO info{};
            info.cbSize = sizeof(info); info.fMask = MIM_BACKGROUND;
            if (!GetMenuInfo(menu, &info)) return false;
            m_menus.push_back({ menu, info.hbrBack });
            info.hbrBack = m_background;
            if (!SetMenuInfo(menu, &info)) return false;
            const int count = GetMenuItemCount(menu);
            if (count < 0) return false;
            for (int position = 0; position < count; ++position)
            {
                MENUITEMINFOW value{};
                value.cbSize = sizeof(value);
                value.fMask = MIIM_FTYPE | MIIM_DATA | MIIM_STRING | MIIM_SUBMENU;
                if (!GetMenuItemInfoW(menu, position, TRUE, &value)) return false;
                auto item = std::unique_ptr<Item>(new Item);
                item->menu = menu; item->position = static_cast<UINT>(position);
                item->originalType = value.fType; item->originalData = value.dwItemData;
                const HMENU submenu = value.hSubMenu;
                item->text.resize(static_cast<size_t>(value.cch) + 1);
                value.dwTypeData = &item->text[0];
                value.cch = static_cast<UINT>(item->text.size());
                if (!GetMenuItemInfoW(menu, position, TRUE, &value)) return false;
                item->text.resize(value.cch);
                item->accessible.dwMSAASignature = MSAA_MENU_SIG;
                item->accessible.cchWText = static_cast<DWORD>(item->text.size());
                item->accessible.pszWText = const_cast<LPWSTR>(item->text.c_str());
                value.fMask = MIIM_FTYPE | MIIM_DATA;
                value.fType = item->originalType | MFT_OWNERDRAW;
                value.dwItemData = reinterpret_cast<ULONG_PTR>(item.get());
                // Keep restoration data before mutation, also on partial failure.
                m_items.push_back(std::move(item));
                if (!SetMenuItemInfoW(menu, position, TRUE, &value)) return false;
                if (submenu && !Attach(submenu)) return false;
            }
            return true;
        }
    public:
        Session(HMENU menu, bool dark, UINT dpi, bool nativeAccessibility = false)
            : m_colors(Colors(dark)), m_dpi(dpi ? dpi : 96)
        {
            if (nativeAccessibility) return; // Full native High Contrast behavior.
            NONCLIENTMETRICSW metrics{};
            metrics.cbSize = sizeof(metrics);
            if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi)) return;
            m_font = CreateFontIndirectW(&metrics.lfMenuFont);
            metrics.lfMenuFont.lfWeight = FW_BOLD;
            m_bold = CreateFontIndirectW(&metrics.lfMenuFont);
            m_background = CreateSolidBrush(m_colors.background);
            if (!m_font || !m_bold || !m_background) return;
            try { m_ready = Attach(menu); }
            catch (...) { Restore(); } // Allocation failure falls back to native.
            if (!m_ready) Restore();
        }
        Session(Session const&) = delete;
        Session& operator=(Session const&) = delete;
        ~Session() { Restore(); }
        bool Ready() const { return m_ready; }
        void Restore()
        {
            for (auto const& item : m_items)
            {
                MENUITEMINFOW value{};
                value.cbSize = sizeof(value); value.fMask = MIIM_FTYPE | MIIM_DATA;
                value.fType = item->originalType; value.dwItemData = item->originalData;
                SetMenuItemInfoW(item->menu, item->position, TRUE, &value);
            }
            for (auto const& menu : m_menus)
            {
                MENUINFO info{};
                info.cbSize = sizeof(info); info.fMask = MIM_BACKGROUND;
                info.hbrBack = menu.originalBackground; SetMenuInfo(menu.handle, &info);
            }
            m_items.clear(); m_menus.clear(); m_ready = false;
            if (m_font) DeleteObject(m_font);
            if (m_bold) DeleteObject(m_bold);
            if (m_background) DeleteObject(m_background);
            m_font = m_bold = nullptr; m_background = nullptr;
        }
        bool Measure(MEASUREITEMSTRUCT* value) const
        {
            if (!value || value->CtlType != ODT_MENU) return false;
            auto const* item = Find(value->itemData);
            if (!item) return false;
            if (item->originalType & MFT_SEPARATOR)
            { value->itemWidth = Px(60); value->itemHeight = Px(9); return true; }
            HDC dc = CreateCompatibleDC(nullptr);
            if (!dc) return false;
            const auto old = SelectObject(dc, m_bold); // Reserve bold default width.
            RECT bounds{};
            DrawTextW(dc, item->text.c_str(), static_cast<int>(item->text.size()), &bounds, DT_CALCRECT | DT_SINGLELINE);
            value->itemWidth = static_cast<UINT>(bounds.right + Px(56));
            value->itemHeight = static_cast<UINT>((bounds.bottom > Px(18) ? bounds.bottom : Px(18)) + Px(10));
            SelectObject(dc, old); DeleteDC(dc);
            return true;
        }
        bool Draw(DRAWITEMSTRUCT const* value) const
        {
            if (!value || value->CtlType != ODT_MENU) return false;
            auto const* item = Find(value->itemData);
            if (!item) return false;
            const int saved = SaveDC(value->hDC);
            if (!saved) return false;
            IntersectClipRect(value->hDC, value->rcItem.left, value->rcItem.top, value->rcItem.right, value->rcItem.bottom);
            const bool disabled = (value->itemState & (ODS_DISABLED | ODS_GRAYED)) != 0;
            const bool separator = (item->originalType & MFT_SEPARATOR) != 0;
            const COLORREF text = disabled ? m_colors.disabled : m_colors.text;
            SetDCBrushColor(value->hDC, !separator && !disabled && (value->itemState & ODS_SELECTED) ? m_colors.hover : m_colors.background);
            FillRect(value->hDC, &value->rcItem, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SelectObject(value->hDC, GetStockObject(DC_PEN));
            SetDCPenColor(value->hDC, separator ? m_colors.separator : text);
            const int center = (value->rcItem.top + value->rcItem.bottom) / 2;
            if (separator)
            {
                MoveToEx(value->hDC, value->rcItem.left + Px(12), center, nullptr);
                LineTo(value->hDC, value->rcItem.right - Px(12), center);
            }
            else
            {
                SelectObject(value->hDC, value->itemState & ODS_DEFAULT ? m_bold : m_font);
                SetBkMode(value->hDC, TRANSPARENT); SetTextColor(value->hDC, text);
                RECT bounds = value->rcItem;
                bounds.left += Px(32); bounds.right -= Px(24);
                DrawTextW(value->hDC, item->text.c_str(), static_cast<int>(item->text.size()), &bounds,
                    DT_SINGLELINE | DT_VCENTER | ((value->itemState & ODS_NOACCEL) ? DT_HIDEPREFIX : 0));
                if (value->itemState & ODS_CHECKED)
                {
                    const int x = value->rcItem.left + Px(12);
                    MoveToEx(value->hDC, x, center, nullptr);
                    LineTo(value->hDC, x + Px(4), center + Px(4));
                    LineTo(value->hDC, x + Px(11), center - Px(4));
                }
                // USER32 provides the native submenu arrow outside rcItem.
                // Keep its navigation/placement rather than drawing a duplicate.
            }
            RestoreDC(value->hDC, saved);
            return true;
        }
        LRESULT MenuChar(wchar_t key, HMENU menu) const
        {
            UINT selected = static_cast<UINT>(-1);
            bool explicitMnemonic = false;
            const auto mnemonicFor = [](std::wstring const& text) -> wchar_t
            {
                for (size_t i = 0; i + 1 < text.size(); ++i)
                    if (text[i] == L'&')
                    {
                        if (text[i + 1] == L'&') { ++i; continue; }
                        return text[i + 1];
                    }
                return 0;
            };
            // Two passes, no allocations inside the window procedure.
            for (auto const& item : m_items)
            {
                if (item->menu != menu || (item->originalType & MFT_SEPARATOR)) continue;
                MENUITEMINFOW state{}; state.cbSize = sizeof(state); state.fMask = MIIM_STATE;
                if (!GetMenuItemInfoW(menu, item->position, TRUE, &state) || (state.fState & MFS_DISABLED)) continue;
                if (state.fState & MFS_HILITE) selected = item->position;
                const wchar_t mnemonic = mnemonicFor(item->text);
                if (mnemonic && towupper(mnemonic) == towupper(key)) explicitMnemonic = true;
            }
            UINT count = 0, first = 0, next = static_cast<UINT>(-1);
            for (auto const& item : m_items)
            {
                if (item->menu != menu || (item->originalType & MFT_SEPARATOR)) continue;
                MENUITEMINFOW state{}; state.cbSize = sizeof(state); state.fMask = MIIM_STATE;
                if (!GetMenuItemInfoW(menu, item->position, TRUE, &state) || (state.fState & MFS_DISABLED)) continue;
                const wchar_t mnemonic = mnemonicFor(item->text);
                const bool match = explicitMnemonic ? mnemonic && towupper(mnemonic) == towupper(key)
                    : !mnemonic && !item->text.empty() && towupper(item->text[0]) == towupper(key);
                if (!match) continue;
                if (count++ == 0) first = item->position;
                if (next == static_cast<UINT>(-1) && (selected == static_cast<UINT>(-1) || item->position > selected)) next = item->position;
            }
            if (!count) return MAKELRESULT(0, MNC_IGNORE);
            // Implicit first-letter keys select, never unexpectedly execute.
            if (explicitMnemonic && count == 1) return MAKELRESULT(first, MNC_EXECUTE);
            return MAKELRESULT(next == static_cast<UINT>(-1) ? first : next, MNC_SELECT);
        }
    };
}
