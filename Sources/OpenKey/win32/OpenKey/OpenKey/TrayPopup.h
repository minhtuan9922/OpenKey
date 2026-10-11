#pragma once
#include "TrayMenuTheme.h"
#include <windowsx.h>
#include <algorithm>
#pragma comment(lib, "oleacc.lib")
#pragma comment(lib, "oleaut32.lib")

namespace OpenKeyTrayPopup
{
    struct Entry
    {
        std::wstring text;
        UINT id = 0, state = 0;
        bool separator = false;
        std::vector<Entry> children;
        bool Enabled() const { return !separator && !(state & MFS_DISABLED); }
    };
    inline bool Snapshot(HMENU menu, std::vector<Entry>& entries, int depth = 0)
    {
        const int count = GetMenuItemCount(menu);
        if (count < 0 || count > 256 || depth > 8) return false;
        for (int i = 0; i < count; ++i)
        {
            MENUITEMINFOW value{}; value.cbSize = sizeof(value);
            value.fMask = MIIM_STRING | MIIM_FTYPE | MIIM_STATE | MIIM_ID | MIIM_SUBMENU;
            if (!GetMenuItemInfoW(menu, i, TRUE, &value)) return false;
            Entry entry; entry.id = value.wID; entry.state = value.fState;
            entry.separator = (value.fType & MFT_SEPARATOR) != 0;
            entry.text.resize(static_cast<size_t>(value.cch) + 1);
            value.dwTypeData = &entry.text[0]; value.cch = static_cast<UINT>(entry.text.size());
            if (!GetMenuItemInfoW(menu, i, TRUE, &value)) return false;
            entry.text.resize(value.cch);
            if (value.hSubMenu && !Snapshot(value.hSubMenu, entry.children, depth + 1)) return false;
            entries.push_back(std::move(entry));
        }
        return true;
    }
    inline int Next(std::vector<Entry> const& entries, int current, int direction)
    {
        const int count = static_cast<int>(entries.size());
        if (!count) return -1;
        for (int step = 0; step < count; ++step)
        {
            current = (current + direction + count) % count;
            if (entries[current].Enabled()) return current;
        }
        return -1;
    }
    inline RECT Fit(POINT origin, int width, int height, RECT work)
    {
        width = (std::min)(width, static_cast<int>(work.right - work.left));
        height = (std::min)(height, static_cast<int>(work.bottom - work.top));
        origin.x = (std::max)(work.left, (std::min)(origin.x, work.right - width));
        origin.y = (std::max)(work.top, (std::min)(origin.y, work.bottom - height));
        return { origin.x, origin.y, origin.x + width, origin.y + height };
    }
    inline std::wstring Name(std::wstring const& text)
    {
        std::wstring result;
        for (size_t i = 0; i < text.size(); ++i)
            if (text[i] != L'&') result += text[i];
            else if (i + 1 < text.size() && text[i + 1] == L'&') { result += L'&'; ++i; }
        return result;
    }
    inline bool ContainsRounded(RECT bounds, POINT point, int radius)
    {
        if (!PtInRect(&bounds, point)) return false;
        radius = (std::min)(radius, static_cast<int>((std::min)(bounds.right - bounds.left, bounds.bottom - bounds.top) / 2));
        const LONG x = point.x < bounds.left + radius ? bounds.left + radius :
            point.x >= bounds.right - radius ? bounds.right - radius - 1 : point.x;
        const LONG y = point.y < bounds.top + radius ? bounds.top + radius :
            point.y >= bounds.bottom - radius ? bounds.bottom - radius - 1 : point.y;
        const LONG dx = point.x - x, dy = point.y - y;
        return dx * dx + dy * dy <= radius * radius;
    }
    constexpr UINT AccessibleAction = WM_APP + 118;
    struct AccessibleData
    {
        HWND window = nullptr;
        std::vector<Entry> const* entries = nullptr;
        // Own the snapshot: clients may keep this object after popup destruction.
        std::shared_ptr<std::vector<Entry>> snapshot;
        std::vector<RECT> locations;
        int selected = -1;
        bool active = false;
    };
    class Accessible final : public IAccessible
    {
        LONG m_refs = 1;
        std::shared_ptr<AccessibleData> m_data;
        int Index(VARIANT child) const
        { return child.vt == VT_I4 && child.lVal >= 0 && child.lVal <= static_cast<LONG>(m_data->entries->size()) ? child.lVal - 1 : -2; }
        HRESULT String(BSTR* output, std::wstring const& value)
        { if (!output) return E_POINTER; *output = SysAllocString(value.c_str()); return *output ? S_OK : E_OUTOFMEMORY; }
    public:
        explicit Accessible(std::shared_ptr<AccessibleData> data) : m_data(std::move(data)) {}
        HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** output) override
        {
            if (!output) return E_POINTER; *output = nullptr;
            if (iid == IID_IUnknown || iid == IID_IDispatch || iid == IID_IAccessible)
            { *output = static_cast<IAccessible*>(this); AddRef(); return S_OK; }
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef() override { return InterlockedIncrement(&m_refs); }
        ULONG STDMETHODCALLTYPE Release() override { const LONG refs = InterlockedDecrement(&m_refs); if (!refs) delete this; return refs; }
        HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* n) override { if (!n) return E_POINTER; *n = 0; return S_OK; }
        HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT, LCID, ITypeInfo** p) override { if (p) *p = nullptr; return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID, LPOLESTR*, UINT, LCID, DISPID*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE Invoke(DISPID, REFIID, LCID, WORD, DISPPARAMS*, VARIANT*, EXCEPINFO*, UINT*) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE get_accParent(IDispatch** p) override { if (!p) return E_POINTER; *p = nullptr; return S_FALSE; }
        HRESULT STDMETHODCALLTYPE get_accChildCount(LONG* p) override { if (!p) return E_POINTER; *p = static_cast<LONG>(m_data->entries->size()); return S_OK; }
        HRESULT STDMETHODCALLTYPE get_accChild(VARIANT, IDispatch** p) override { if (!p) return E_POINTER; *p = nullptr; return S_FALSE; }
        HRESULT STDMETHODCALLTYPE get_accName(VARIANT child, BSTR* p) override
        {
            if (!p) return E_POINTER; *p = nullptr;
            const int i = Index(child); if (i == -2) return E_INVALIDARG;
            try { return String(p, i < 0 ? L"OpenKey" : Name((*m_data->entries)[i].text)); }
            catch (...) { return E_OUTOFMEMORY; }
        }
        HRESULT STDMETHODCALLTYPE get_accValue(VARIANT, BSTR* p) override { if (!p) return E_POINTER; *p = nullptr; return S_FALSE; }
        HRESULT STDMETHODCALLTYPE get_accDescription(VARIANT v, BSTR* p) override { return get_accValue(v, p); }
        HRESULT STDMETHODCALLTYPE get_accRole(VARIANT child, VARIANT* p) override
        {
            if (!p) return E_POINTER; VariantInit(p); const int i = Index(child); if (i == -2) return E_INVALIDARG;
            p->vt = VT_I4; p->lVal = i < 0 ? ROLE_SYSTEM_MENUPOPUP : ((*m_data->entries)[i].separator ? ROLE_SYSTEM_SEPARATOR : ROLE_SYSTEM_MENUITEM); return S_OK;
        }
        HRESULT STDMETHODCALLTYPE get_accState(VARIANT child, VARIANT* p) override
        {
            if (!p) return E_POINTER; VariantInit(p); const int i = Index(child); if (i == -2) return E_INVALIDARG;
            p->vt = VT_I4;
            if (!m_data->window) p->lVal |= STATE_SYSTEM_INVISIBLE;
            if (i >= 0)
            {
                auto const& entry = (*m_data->entries)[i];
                if (!entry.separator) p->lVal |= STATE_SYSTEM_FOCUSABLE | STATE_SYSTEM_SELECTABLE;
                if (!entry.Enabled()) p->lVal |= STATE_SYSTEM_UNAVAILABLE;
                if (entry.state & MFS_CHECKED) p->lVal |= STATE_SYSTEM_CHECKED;
                if (!entry.children.empty()) p->lVal |= STATE_SYSTEM_HASPOPUP;
                if (m_data->active && i == m_data->selected) p->lVal |= STATE_SYSTEM_FOCUSED | STATE_SYSTEM_SELECTED;
                if (i >= static_cast<int>(m_data->locations.size()) || IsRectEmpty(&m_data->locations[i])) p->lVal |= STATE_SYSTEM_OFFSCREEN;
            }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE get_accHelp(VARIANT v, BSTR* p) override { return get_accValue(v, p); }
        HRESULT STDMETHODCALLTYPE get_accHelpTopic(BSTR* p, VARIANT, LONG* n) override { if (p) *p = nullptr; if (n) *n = -1; return S_FALSE; }
        HRESULT STDMETHODCALLTYPE get_accKeyboardShortcut(VARIANT v, BSTR* p) override { return get_accValue(v, p); }
        HRESULT STDMETHODCALLTYPE get_accFocus(VARIANT* p) override
        { if (!p) return E_POINTER; VariantInit(p); if (!m_data->active || !m_data->window) return S_FALSE; p->vt = VT_I4; p->lVal = m_data->selected + 1; return S_OK; }
        HRESULT STDMETHODCALLTYPE get_accSelection(VARIANT* p) override { return get_accFocus(p); }
        HRESULT STDMETHODCALLTYPE get_accDefaultAction(VARIANT child, BSTR* p) override
        { const int i = Index(child); if (i < 0) { if (p) *p = nullptr; return S_FALSE; } return String(p, (*m_data->entries)[i].children.empty() ? L"Chọn" : L"Mở"); }
        HRESULT STDMETHODCALLTYPE accSelect(LONG flags, VARIANT child) override
        {
            const int i = Index(child); if (i < 0 || !(*m_data->entries)[i].Enabled()) return E_INVALIDARG;
            if (!(flags & (SELFLAG_TAKEFOCUS | SELFLAG_TAKESELECTION))) return E_INVALIDARG;
            return m_data->window && PostMessageW(m_data->window, AccessibleAction, i, 0) ? S_OK : S_FALSE;
        }
        HRESULT STDMETHODCALLTYPE accLocation(LONG* x, LONG* y, LONG* w, LONG* h, VARIANT child) override
        {
            if (!x || !y || !w || !h) return E_POINTER; *x = *y = *w = *h = 0;
            const int i = Index(child); if (i == -2) return E_INVALIDARG;
            if (!m_data->window) return S_FALSE;
            RECT rect{}; if (i < 0) GetWindowRect(m_data->window, &rect);
            else if (i < static_cast<int>(m_data->locations.size())) rect = m_data->locations[i];
            *x = rect.left; *y = rect.top; *w = rect.right - rect.left; *h = rect.bottom - rect.top; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE accNavigate(LONG direction, VARIANT from, VARIANT* p) override
        {
            if (!p) return E_POINTER; VariantInit(p); const int i = Index(from); if (i == -2) return E_INVALIDARG;
            int target = -1; const int count = static_cast<int>(m_data->entries->size());
            if (direction == NAVDIR_FIRSTCHILD && i < 0 && count) target = 0;
            else if (direction == NAVDIR_LASTCHILD && i < 0 && count) target = count - 1;
            else if (direction == NAVDIR_NEXT && i >= 0 && i + 1 < count) target = i + 1;
            else if (direction == NAVDIR_PREVIOUS && i > 0) target = i - 1;
            if (target < 0) return S_FALSE; p->vt = VT_I4; p->lVal = target + 1; return S_OK;
        }
        HRESULT STDMETHODCALLTYPE accHitTest(LONG x, LONG y, VARIANT* p) override
        {
            if (!p) return E_POINTER; VariantInit(p); if (!m_data->window) return S_FALSE;
            POINT point{ x,y }; RECT bounds{}; GetWindowRect(m_data->window, &bounds); if (!PtInRect(&bounds, point)) return S_FALSE;
            p->vt = VT_I4; p->lVal = CHILDID_SELF;
            for (size_t i = 0; i < m_data->locations.size(); ++i) if (PtInRect(&m_data->locations[i], point)) { p->lVal = static_cast<LONG>(i + 1); break; }
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE accDoDefaultAction(VARIANT child) override
        { const int i = Index(child); if (i < 0 || !(*m_data->entries)[i].Enabled()) return E_INVALIDARG; return m_data->window && PostMessageW(m_data->window, AccessibleAction, i, 1) ? S_OK : S_FALSE; }
        HRESULT STDMETHODCALLTYPE put_accName(VARIANT, BSTR) override { return E_NOTIMPL; }
        HRESULT STDMETHODCALLTYPE put_accValue(VARIANT, BSTR) override { return E_NOTIMPL; }
    };

    struct Result { bool handled = false; UINT command = 0; };
    class Popup
    {
        friend struct TestAccess; // Offscreen regression seam; never opens windows.
        struct Panel
        {
            Popup* owner;
            HWND window = nullptr;
            std::shared_ptr<AccessibleData> data;
            Accessible* accessible = nullptr;
            RECT bounds{}, viewport{};
            std::vector<RECT> rows;
            int width = 0, height = 0, total = 0, scroll = 0;
            HDC buffer = nullptr;
            HBITMAP bitmap = nullptr;
            HGDIOBJ oldBitmap = nullptr;
            ~Panel()
            {
                if (window) { NotifyWinEvent(EVENT_SYSTEM_MENUPOPUPEND, window, OBJID_CLIENT, CHILDID_SELF); DestroyWindow(window); }
                if (data) { data->window = nullptr; data->active = false; }
                if (accessible) accessible->Release();
                if (buffer) { if (oldBitmap) SelectObject(buffer, oldBitmap); DeleteDC(buffer); }
                if (bitmap) DeleteObject(bitmap);
            }
        };
        std::shared_ptr<std::vector<Entry>> m_snapshot = std::make_shared<std::vector<Entry>>();
        std::vector<std::unique_ptr<Panel>> m_panels;
        OpenKeyTrayTheme::Palette m_colors;
        HFONT m_font = nullptr, m_bold = nullptr;
        UINT m_dpi;
        bool m_done = false;
        UINT m_command = 0;
        static constexpr UINT_PTR HoverTimer = 1;
        static constexpr ULONGLONG HoverDelay = 200;
        bool m_hoverPending = false;
        size_t m_hoverPanel = 0;
        int m_hoverItem = -1;
        ULONGLONG m_hoverDue = 0;
        int Px(int value) const { return MulDiv(value, static_cast<int>(m_dpi), 96); }
        static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w, LPARAM l)
        {
            auto* panel = reinterpret_cast<Panel*>(GetWindowLongPtrW(window, GWLP_USERDATA));
            if (message == WM_NCCREATE)
            {
                panel = static_cast<Panel*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
                panel->window = window; panel->data->window = window;
                SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(panel));
            }
            if (!panel) return DefWindowProcW(window, message, w, l);
            auto* self = panel->owner;
            try {
            switch (message)
            {
            case WM_ERASEBKGND: return 1;
            case WM_PAINT: { PAINTSTRUCT paint{}; HDC dc = BeginPaint(window, &paint); self->BufferedPaint(*panel, dc, paint.rcPaint); EndPaint(window, &paint); return 0; }
            case WM_GETOBJECT:
                if (static_cast<DWORD>(l) == static_cast<DWORD>(OBJID_CLIENT) && panel->accessible)
                    return LresultFromObject(IID_IAccessible, w, panel->accessible);
                break;
            case WM_MOUSEMOVE: self->Mouse(false); return 0;
            case WM_TIMER: if (w == HoverTimer) { if (self->m_hoverPending) { self->Mouse(false); self->FlushHover(GetTickCount64()); } return 0; } break;
            case WM_LBUTTONUP: self->Mouse(true); return 0;
            case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: self->Outside(); return 0;
            case WM_MOUSEWHEEL: self->Scroll(GET_WHEEL_DELTA_WPARAM(w) > 0 ? -self->Px(80) : self->Px(80)); return 0;
            case WM_KEYDOWN: self->Key(static_cast<UINT>(w)); return 0;
            case WM_CHAR: self->Character(static_cast<wchar_t>(w)); return 0;
            case WM_ACTIVATE: if (LOWORD(w) == WA_INACTIVE && !self->m_panels.empty() && panel == self->m_panels.front().get()) self->m_done = true; return 0;
            case WM_CAPTURECHANGED: if (reinterpret_cast<HWND>(l) != window) self->m_done = true; return 0;
            case WM_CANCELMODE: case WM_CLOSE: case WM_DISPLAYCHANGE: case WM_SETTINGCHANGE: self->m_done = true; return 0;
            case AccessibleAction:
                self->AccessibleSelect(panel, static_cast<int>(w), l != 0); return 0;
            case WM_NCDESTROY:
                panel->window = nullptr; panel->data->window = nullptr;
                SetWindowLongPtrW(window, GWLP_USERDATA, 0); break;
            }
            return DefWindowProcW(window, message, w, l);
            } catch (...) { self->m_done = true; return 0; } // Never unwind through USER32.
        }
        void Locations(Panel& panel)
        {
            panel.data->locations.clear();
            for (auto row : panel.rows)
            {
                OffsetRect(&row, 0, panel.viewport.top - panel.scroll);
                RECT clipped{}; IntersectRect(&clipped, &row, &panel.viewport);
                if (!IsRectEmpty(&clipped)) OffsetRect(&clipped, panel.bounds.left, panel.bounds.top);
                panel.data->locations.push_back(clipped);
            }
        }
        bool Open(std::vector<Entry> const& entries, POINT point, HWND owner, RECT work, RECT const* parentBounds = nullptr)
        {
            auto panel = std::unique_ptr<Panel>(new Panel);
            panel->owner = this; panel->data = std::make_shared<AccessibleData>();
            panel->data->snapshot = m_snapshot; panel->data->entries = &entries;
            HDC dc = CreateCompatibleDC(nullptr); if (!dc) return false;
            auto old = SelectObject(dc, m_bold);
            TEXTMETRICW metrics{}; GetTextMetricsW(dc, &metrics);
            int width = Px(160), y = 0;
            for (auto const& entry : entries)
            {
                RECT text{}; DrawTextW(dc, entry.text.c_str(), static_cast<int>(entry.text.size()), &text, DT_CALCRECT | DT_SINGLELINE);
                width = (std::max)(width, static_cast<int>(text.right) + Px(76));
                const int height = entry.separator ? Px(9) : (std::max)(Px(32), static_cast<int>(metrics.tmHeight) + Px(12));
                panel->rows.push_back({ 0,y,0,y + height }); y += height;
            }
            SelectObject(dc, old); DeleteDC(dc);
            if (parentBounds && point.x + width > work.right) point.x = parentBounds->left - width + Px(2);
            panel->total = y; panel->bounds = Fit(point, width, y + Px(20), work);
            panel->width = panel->bounds.right - panel->bounds.left;
            panel->height = panel->bounds.bottom - panel->bounds.top;
            const bool overflow = panel->total + Px(20) > panel->height;
            const int padding = Px(overflow ? 24 : 10);
            panel->viewport = { 0,padding,panel->width,panel->height - padding };
            if (panel->viewport.bottom <= panel->viewport.top) return false;
            for (auto& row : panel->rows) row.right = panel->width;
            panel->accessible = new Accessible(panel->data);
            // The notification overflow flyout is topmost. Every menu panel
            // must share that band, including no-activate submenus.
            HWND window = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW | (m_panels.empty() ? 0 : WS_EX_NOACTIVATE),
                L"OpenKey.RoundedTrayPopup", L"OpenKey", WS_POPUP,
                panel->bounds.left, panel->bounds.top, panel->width, panel->height,
                owner, nullptr, GetModuleHandleW(nullptr), panel.get());
            if (!window) return false;
            HRGN region = CreateRoundRectRgn(0, 0, panel->width + 1, panel->height + 1, Px(24), Px(24));
            if (!region || !SetWindowRgn(window, region, FALSE))
            { if (region) DeleteObject(region); return false; } // USER32 owns successful region.
            Locations(*panel);
            m_panels.push_back(std::move(panel));
            if (!SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW)) return false;
            NotifyWinEvent(EVENT_SYSTEM_MENUPOPUPSTART, window, OBJID_CLIENT, CHILDID_SELF);
            return true;
        }
        void CloseAfter(size_t index)
        { while (m_panels.size() > index + 1) m_panels.pop_back(); }
        void Select(size_t panelIndex, int selected)
        {
            auto& panel = *m_panels[panelIndex];
            if (selected < 0 || selected >= static_cast<int>(panel.rows.size()) || !(*panel.data->entries)[selected].Enabled()) return;
            const bool changed = selected != panel.data->selected || !panel.data->active;
            const int previous = panel.data->selected, oldScroll = panel.scroll;
            const auto row = panel.rows[selected]; const int visible = panel.viewport.bottom - panel.viewport.top;
            if (row.top < panel.scroll) panel.scroll = row.top;
            else if (row.bottom > panel.scroll + visible) panel.scroll = row.bottom - visible;
            if (!changed && panel.scroll == oldScroll) return;
            if (selected != panel.data->selected) { CloseAfter(panelIndex); panel.data->selected = selected; }
            for (auto& p : m_panels) p->data->active = p.get() == &panel;
            if (panel.scroll != oldScroll) Locations(panel);
            if (panel.window) {
                if (panel.scroll != oldScroll) InvalidateRect(panel.window, nullptr, FALSE);
                else if (previous != selected)
                {
                    for (int item : { previous, selected }) if (item >= 0)
                    {
                        RECT rowBounds = panel.rows[item]; OffsetRect(&rowBounds, 0, panel.viewport.top - panel.scroll);
                        RECT clipped{}; if (IntersectRect(&clipped, &rowBounds, &panel.viewport)) InvalidateRect(panel.window, &clipped, FALSE);
                    }
                }
                if (changed) NotifyWinEvent(EVENT_OBJECT_FOCUS, panel.window, OBJID_CLIENT, selected + 1);
            }
        }
        void Activate(size_t index, bool keyboard)
        {
            auto& panel = *m_panels[index]; const int selected = panel.data->selected;
            if (selected < 0) return;
            auto const& entry = (*panel.data->entries)[selected]; if (!entry.Enabled()) return;
            if (entry.children.empty()) { m_command = entry.id; m_done = true; return; }
            if (m_panels.size() > index + 1) { if (keyboard) Select(index + 1, Next(entry.children, -1, 1)); return; }
            MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
            GetMonitorInfoW(MonitorFromWindow(panel.window, MONITOR_DEFAULTTONEAREST), &monitor);
            POINT point{ panel.bounds.right - Px(2), panel.bounds.top + panel.viewport.top + panel.rows[selected].top - panel.scroll };
            if (!Open(entry.children, point, m_panels.front()->window, monitor.rcWork, &panel.bounds)) { m_done = true; return; }
            if (keyboard) Select(index + 1, Next(entry.children, -1, 1));
        }
        int Hit(Panel const& panel, POINT screen) const
        {
            POINT local{ screen.x - panel.bounds.left, screen.y - panel.bounds.top };
            if (!PtInRect(&panel.viewport, local)) return -1;
            local.y += panel.scroll - panel.viewport.top;
            for (size_t i = 0; i < panel.rows.size(); ++i) if (PtInRect(&panel.rows[i], local)) return static_cast<int>(i);
            return -1;
        }
        void CancelHover()
        {
            if (!m_hoverPending) return;
            m_hoverPending = false;
            if (!m_panels.empty() && m_panels.front()->window) KillTimer(m_panels.front()->window, HoverTimer);
        }
        void Hover(size_t index, int item, ULONGLONG now)
        {
            auto& panel = *m_panels[index];
            const bool hasChild = m_panels.size() > index + 1;
            const bool submenu = !(*panel.data->entries)[item].children.empty();
            if (hasChild && panel.data->selected == item) { CancelHover(); Select(index, item); return; }
            // Keep the child alive briefly while crossing another parent row diagonally.
            if (!hasChild) Select(index, item);
            if (!hasChild && !submenu) { CancelHover(); return; }
            if (m_hoverPending && m_hoverPanel == index && m_hoverItem == item) return;
            CancelHover(); m_hoverPending = true; m_hoverPanel = index; m_hoverItem = item; m_hoverDue = now + HoverDelay;
            if (m_panels.front()->window && !SetTimer(m_panels.front()->window, HoverTimer, 30, nullptr))
                FlushHover(m_hoverDue);
        }
        void FlushHover(ULONGLONG now)
        {
            if (!m_hoverPending || now < m_hoverDue) return;
            const size_t index = m_hoverPanel; const int item = m_hoverItem; CancelHover();
            if (index >= m_panels.size() || item < 0 || item >= static_cast<int>(m_panels[index]->rows.size())) return;
            Select(index, item);
            if (!(*m_panels[index]->data->entries)[item].children.empty()) Activate(index, false);
        }
        void Mouse(bool activate)
        {
            POINT point{}; GetCursorPos(&point);
            for (size_t n = m_panels.size(); n > 0; --n)
            {
                auto& panel = *m_panels[n - 1]; if (!ContainsRounded(panel.bounds, point, Px(12))) continue;
                const int item = Hit(panel, point);
                if (item >= 0 && (*panel.data->entries)[item].Enabled())
                {
                    if (activate) { CancelHover(); Select(n - 1, item); Activate(n - 1, false); }
                    else Hover(n - 1, item, GetTickCount64());
                }
                else if (activate && item < 0 && panel.total > panel.viewport.bottom - panel.viewport.top)
                    Scroll(point.y < panel.bounds.top + panel.viewport.top ? -Px(80) : Px(80), n - 1);
                else CancelHover();
                return;
            }
            CancelHover(); if (activate) m_done = true;
        }
        void Outside()
        {
            POINT point{}; GetCursorPos(&point);
            for (auto const& panel : m_panels) if (ContainsRounded(panel->bounds, point, Px(12))) return;
            m_done = true;
        }
        void Scroll(int delta, size_t index = static_cast<size_t>(-1))
        {
            CancelHover();
            if (m_panels.empty()) return;
            if (index == static_cast<size_t>(-1)) index = m_panels.size() - 1;
            auto& panel = *m_panels[index]; CloseAfter(index);
            const int maximum = (std::max)(0, panel.total - static_cast<int>(panel.viewport.bottom - panel.viewport.top));
            panel.scroll = (std::max)(0, (std::min)(maximum, panel.scroll + delta));
            Locations(panel); if (panel.window) InvalidateRect(panel.window, nullptr, FALSE);
        }
        void Key(UINT key)
        {
            CancelHover();
            if (m_panels.empty()) return;
            const size_t index = m_panels.size() - 1; auto& panel = *m_panels[index];
            auto const& entries = *panel.data->entries;
            if (key == VK_ESCAPE || key == VK_LEFT)
            { if (index) { m_panels.pop_back(); Select(index - 1, m_panels.back()->data->selected); } else if (key == VK_ESCAPE) m_done = true; return; }
            if (key == VK_RIGHT)
            { if (panel.data->selected >= 0 && !entries[panel.data->selected].children.empty()) Activate(index, true); return; }
            if (key == VK_RETURN || key == VK_SPACE) { Activate(index, true); return; }
            if (key == VK_TAB) { m_done = true; return; }
            int next = panel.data->selected;
            if (key == VK_HOME) next = Next(entries, -1, 1);
            else if (key == VK_END) next = Next(entries, 0, -1);
            else if (key == VK_UP || key == VK_DOWN || key == VK_PRIOR || key == VK_NEXT)
            {
                const int direction = key == VK_UP || key == VK_PRIOR ? -1 : 1;
                for (int i = 0; i < (key == VK_PRIOR || key == VK_NEXT ? 5 : 1); ++i)
                    next = Next(entries, next < 0 && direction < 0 ? 0 : next, direction);
            }
            else return;
            Select(index, next);
        }
        void Character(wchar_t key)
        {
            CancelHover();
            if (key < L' ') return;
            const size_t index = m_panels.size() - 1; auto& panel = *m_panels[index]; auto const& entries = *panel.data->entries;
            const int count = static_cast<int>(entries.size());
            for (int step = 1; step <= count; ++step)
            {
                const int i = (panel.data->selected + step + count) % count;
                const auto name = Name(entries[i].text);
                if (entries[i].Enabled() && !name.empty() && towupper(name[0]) == towupper(key)) { Select(index, i); return; }
            }
        }
        void AccessibleSelect(Panel* panel, int item, bool activate)
        {
            CancelHover();
            if (item < 0 || item >= static_cast<int>(panel->data->entries->size()) || !(*panel->data->entries)[item].Enabled()) return;
            for (size_t i = 0; i < m_panels.size(); ++i) if (m_panels[i].get() == panel)
            { Select(i, item); if (activate) Activate(i, true); return; }
        }
        void BufferedPaint(Panel& panel, HDC dc, RECT dirty)
        {
            if (!dc || IsRectEmpty(&dirty)) return;
            // Cache one back buffer per fixed-size panel; present the completed frame once.
            if (!panel.buffer)
            {
                HDC buffer = CreateCompatibleDC(dc);
                HBITMAP bitmap = buffer ? CreateCompatibleBitmap(dc, panel.width, panel.height) : nullptr;
                HGDIOBJ old = bitmap ? SelectObject(buffer, bitmap) : nullptr;
                if (!old || old == HGDI_ERROR)
                { if (buffer) DeleteDC(buffer); if (bitmap) DeleteObject(bitmap); Paint(panel, dc); return; }
                panel.buffer = buffer; panel.bitmap = bitmap; panel.oldBitmap = old;
            }
            Paint(panel, panel.buffer);
            if (!BitBlt(dc, dirty.left, dirty.top, dirty.right - dirty.left, dirty.bottom - dirty.top,
                panel.buffer, dirty.left, dirty.top, SRCCOPY)) Paint(panel, dc);
        }
        void Paint(Panel const& panel, HDC dc)
        {
            const int saved = SaveDC(dc); if (!saved) return;
            RECT client{ 0,0,panel.width,panel.height };
            SetDCBrushColor(dc, m_colors.background); FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
            SetBkMode(dc, TRANSPARENT); SelectObject(dc, GetStockObject(DC_PEN));
            IntersectClipRect(dc, panel.viewport.left, panel.viewport.top, panel.viewport.right, panel.viewport.bottom);
            for (size_t i = 0; i < panel.rows.size(); ++i)
            {
                auto row = panel.rows[i]; OffsetRect(&row, 0, panel.viewport.top - panel.scroll);
                auto const& entry = (*panel.data->entries)[i];
                const int center = (row.top + row.bottom) / 2;
                if (entry.separator)
                { SetDCPenColor(dc, m_colors.separator); MoveToEx(dc, Px(12), center, nullptr); LineTo(dc, panel.width - Px(12), center); continue; }
                const COLORREF color = entry.Enabled() ? m_colors.text : m_colors.disabled;
                if (static_cast<int>(i) == panel.data->selected && entry.Enabled())
                {
                    SelectObject(dc, GetStockObject(DC_BRUSH)); SetDCBrushColor(dc, m_colors.hover); SetDCPenColor(dc, m_colors.hover);
                    RoundRect(dc, Px(5), row.top + Px(2), panel.width - Px(5), row.bottom - Px(2), Px(8), Px(8));
                }
                SetDCPenColor(dc, color); SetTextColor(dc, color);
                SelectObject(dc, entry.state & MFS_DEFAULT ? m_bold : m_font);
                RECT text = row; text.left += Px(36); text.right -= Px(30);
                DrawTextW(dc, entry.text.c_str(), static_cast<int>(entry.text.size()), &text, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);
                if (entry.state & MFS_CHECKED)
                { MoveToEx(dc, Px(13), center, nullptr); LineTo(dc, Px(17), center + Px(4)); LineTo(dc, Px(24), center - Px(4)); }
                if (!entry.children.empty())
                { const int x = panel.width - Px(18); MoveToEx(dc, x - Px(3), center - Px(4), nullptr); LineTo(dc, x + Px(1), center); LineTo(dc, x - Px(3), center + Px(4)); }
            }
            RestoreDC(dc, saved);
            if (panel.total > panel.viewport.bottom - panel.viewport.top)
            {
                const int state = SaveDC(dc); SelectObject(dc, GetStockObject(DC_PEN)); SetDCPenColor(dc, m_colors.text);
                const int x = panel.width / 2;
                if (panel.scroll > 0) { MoveToEx(dc, x - Px(4), Px(16), nullptr); LineTo(dc, x, Px(12)); LineTo(dc, x + Px(4), Px(16)); }
                if (panel.scroll + panel.viewport.bottom - panel.viewport.top < panel.total)
                { MoveToEx(dc, x - Px(4), panel.height - Px(16), nullptr); LineTo(dc, x, panel.height - Px(12)); LineTo(dc, x + Px(4), panel.height - Px(16)); }
                RestoreDC(dc, state);
            }
        }
    public:
        Popup(bool dark, UINT dpi) : m_colors(OpenKeyTrayTheme::Colors(dark)), m_dpi(dpi ? dpi : 96) {}
        ~Popup()
        {
            m_done = true;
            if (!m_panels.empty() && m_panels.front()->window && GetCapture() == m_panels.front()->window) ReleaseCapture();
            while (!m_panels.empty()) m_panels.pop_back();
            if (m_font) DeleteObject(m_font); if (m_bold) DeleteObject(m_bold);
        }
        Result Run(HMENU menu, HWND owner, POINT point)
        {
            if (!Snapshot(menu, *m_snapshot) || m_snapshot->empty()) return {};
            NONCLIENTMETRICSW metrics{}; metrics.cbSize = sizeof(metrics);
            if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, m_dpi)) return {};
            m_font = CreateFontIndirectW(&metrics.lfMenuFont); metrics.lfMenuFont.lfWeight = FW_BOLD; m_bold = CreateFontIndirectW(&metrics.lfMenuFont);
            if (!m_font || !m_bold) return {};
            WNDCLASSEXW cls{}; cls.cbSize = sizeof(cls); cls.lpfnWndProc = WindowProc;
            cls.hInstance = GetModuleHandleW(nullptr); cls.lpszClassName = L"OpenKey.RoundedTrayPopup";
            cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); cls.style = CS_DROPSHADOW;
            if (!RegisterClassExW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return {};
            MONITORINFO monitor{}; monitor.cbSize = sizeof(monitor);
            if (!GetMonitorInfoW(MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST), &monitor)) return {};
            if (!Open(*m_snapshot, point, owner, monitor.rcWork)) return {};
            HWND root = m_panels.front()->window;
            SetForegroundWindow(root); SetFocus(root); SetCapture(root);
            if (GetCapture() != root || GetForegroundWindow() != root) return {};
            NotifyWinEvent(EVENT_SYSTEM_MENUSTART, root, OBJID_CLIENT, CHILDID_SELF);
            MSG message{};
            while (!m_done)
            {
                const int status = GetMessageW(&message, nullptr, 0, 0);
                if (status <= 0) { if (!status) PostQuitMessage(static_cast<int>(message.wParam)); break; }
                TranslateMessage(&message); DispatchMessageW(&message);
            }
            if (GetCapture() == root) ReleaseCapture();
            NotifyWinEvent(EVENT_SYSTEM_MENUEND, root, OBJID_CLIENT, CHILDID_SELF);
            while (!m_panels.empty()) m_panels.pop_back();
            PostMessageW(owner, WM_NULL, 0, 0);
            return { true,m_command };
        }
    };
    inline Result Show(HMENU menu, HWND owner, POINT point, bool dark, UINT dpi)
    {
        struct ComScope
        {
            HRESULT status = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            ~ComScope() { if (SUCCEEDED(status)) CoUninitialize(); }
        } com;
        if (FAILED(com.status)) return {}; // Accessible marshaling needs this STA.
        struct DpiScope
        {
            DPI_AWARENESS_CONTEXT previous = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            ~DpiScope() { if (previous) SetThreadDpiAwarenessContext(previous); }
        } scope;
        GetCursorPos(&point);
        if (scope.previous) dpi = OpenKeyTrayTheme::CursorDpi(point);
        try { Popup popup(dark, dpi); return popup.Run(menu, owner, point); }
        catch (...) { return {}; } // Native menu fallback on allocation/init failure.
    }
}
