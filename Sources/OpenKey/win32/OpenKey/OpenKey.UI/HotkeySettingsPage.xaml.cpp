#include "pch.h"
#include "HotkeySettingsPage.xaml.h"
#include "Localization.h"

#include <array>
#include <chrono>

#if __has_include("HotkeySettingsPage.g.cpp")
#include "HotkeySettingsPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    constexpr int ControlMask = 0x100;
    constexpr int AltMask = 0x200;
    constexpr int WindowsMask = 0x400;
    constexpr int ShiftMask = 0x800;
    constexpr int BeepMask = 0x8000;
    constexpr int EmptyKey = 0xFE;

    struct HotkeyChoice
    {
        wchar_t const* label;
        int code;
    };

    constexpr std::array<HotkeyChoice, 49> HotkeyChoices{{
        { L"Không dùng phím ký tự", EmptyKey },
        { L"Space", VK_SPACE },
        { L"A", 'A' }, { L"B", 'B' }, { L"C", 'C' }, { L"D", 'D' },
        { L"E", 'E' }, { L"F", 'F' }, { L"G", 'G' }, { L"H", 'H' },
        { L"I", 'I' }, { L"J", 'J' }, { L"K", 'K' }, { L"L", 'L' },
        { L"M", 'M' }, { L"N", 'N' }, { L"O", 'O' }, { L"P", 'P' },
        { L"Q", 'Q' }, { L"R", 'R' }, { L"S", 'S' }, { L"T", 'T' },
        { L"U", 'U' }, { L"V", 'V' }, { L"W", 'W' }, { L"X", 'X' },
        { L"Y", 'Y' }, { L"Z", 'Z' },
        { L"0", '0' }, { L"1", '1' }, { L"2", '2' }, { L"3", '3' },
        { L"4", '4' }, { L"5", '5' }, { L"6", '6' }, { L"7", '7' },
        { L"8", '8' }, { L"9", '9' },
        { L"`", VK_OEM_3 }, { L"-", VK_OEM_MINUS }, { L"=", VK_OEM_PLUS },
        { L"[", VK_OEM_4 }, { L"]", VK_OEM_6 }, { L"\\", VK_OEM_5 },
        { L";", VK_OEM_1 }, { L"'", VK_OEM_7 }, { L",", VK_OEM_COMMA },
        { L".", VK_OEM_PERIOD }, { L"/", VK_OEM_2 }
    }};
}

namespace winrt::OpenKeyUI::implementation
{
    HotkeySettingsPage::HotkeySettingsPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        PopulateKeys();

        ApplyHotkey(m_preferences.switchKeyStatus);
        m_loadingPreferences = false;
        m_refreshTimer = DispatcherQueue().CreateTimer();
        m_refreshTimer.Interval(std::chrono::milliseconds(500));
        m_refreshTimer.Tick([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get()) page->RefreshPreferences();
        });
        Loaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get())
            {
                page->RefreshPreferences();
                page->m_refreshTimer.Start();
            }
        });
        Unloaded([weak = get_weak()](auto const&, auto const&)
        {
            if (auto page = weak.get()) page->m_refreshTimer.Stop();
        });
    }

    HotkeySettingsPage::~HotkeySettingsPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void HotkeySettingsPage::RefreshPreferences()
    {
        if (m_saveFailed || KeyCombo().IsDropDownOpen()) return;
        const int previousStatus = m_preferences.switchKeyStatus;
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        if (previousStatus == m_preferences.switchKeyStatus) return;
        m_loadingPreferences = true;
        ApplyHotkey(m_preferences.switchKeyStatus);
        m_loadingPreferences = false;
    }

    void HotkeySettingsPage::ApplyHotkey(const int status)
    {
        CtrlCheckBox().IsChecked((status & ControlMask) != 0);
        AltCheckBox().IsChecked((status & AltMask) != 0);
        WinCheckBox().IsChecked((status & WindowsMask) != 0);
        ShiftCheckBox().IsChecked((status & ShiftMask) != 0);
        BeepToggle().IsOn((status & BeepMask) != 0);

        const int currentKey = status & 0xFF;
        auto selected = std::find(m_keyCodes.begin(), m_keyCodes.end(), currentKey);
        if (selected == m_keyCodes.end())
        {
            KeyCombo().Items().Append(box_value(OpenKeyUILocalization::GetString(L"CurrentKeyPrefix") +
                to_hstring(currentKey) + L")"));
            m_keyCodes.push_back(currentKey);
            KeyCombo().SelectedIndex(static_cast<int>(m_keyCodes.size() - 1));
        }
        else
        {
            KeyCombo().SelectedIndex(static_cast<int>(std::distance(m_keyCodes.begin(), selected)));
        }

        UpdatePreview();
    }

    void HotkeySettingsPage::PopulateKeys()
    {
        for (const auto& choice : HotkeyChoices)
        {
            const auto label = choice.code == EmptyKey
                ? OpenKeyUILocalization::GetString(L"NoCharacterKey")
                : hstring(choice.label);
            KeyCombo().Items().Append(box_value(label));
            m_keyCodes.push_back(choice.code);
        }
    }

    void HotkeySettingsPage::OnHotkeyChanged(IInspectable const&, RoutedEventArgs const&)
    {
        SaveHotkey();
    }

    void HotkeySettingsPage::OnKeyChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        SaveHotkey();
    }

    void HotkeySettingsPage::SaveHotkey()
    {
        if (m_loadingPreferences || KeyCombo().SelectedIndex() < 0)
        {
            return;
        }

        const int keyCode = m_keyCodes.at(static_cast<size_t>(KeyCombo().SelectedIndex()));
        int status = keyCode | (keyCode << 24);
        status |= CtrlCheckBox().IsChecked().Value() ? ControlMask : 0;
        status |= AltCheckBox().IsChecked().Value() ? AltMask : 0;
        status |= WinCheckBox().IsChecked().Value() ? WindowsMask : 0;
        status |= ShiftCheckBox().IsChecked().Value() ? ShiftMask : 0;
        status |= BeepToggle().IsOn() ? BeepMask : 0;
        m_preferences.switchKeyStatus = status;

        const bool saved = SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        m_saveFailed = !saved;
        SettingsStatus().IsOpen(true);
        SettingsStatus().Severity(saved
            ? Controls::InfoBarSeverity::Success
            : Controls::InfoBarSeverity::Error);
        SettingsStatus().Title(saved
            ? OpenKeyUILocalization::GetString(L"HotkeySaveSuccessTitle")
            : OpenKeyUILocalization::GetString(L"HotkeySaveFailureTitle"));
        SettingsStatus().Message(saved
            ? OpenKeyUILocalization::GetString(L"HotkeySaveSuccessMessage")
            : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
        UpdatePreview();
    }

    void HotkeySettingsPage::UpdatePreview()
    {
        hstring preview;
        if (CtrlCheckBox().IsChecked().Value()) preview = preview + L"Ctrl + ";
        if (AltCheckBox().IsChecked().Value()) preview = preview + L"Alt + ";
        if (WinCheckBox().IsChecked().Value()) preview = preview + L"Win + ";
        if (ShiftCheckBox().IsChecked().Value()) preview = preview + L"Shift + ";

        const int selectedIndex = KeyCombo().SelectedIndex();
        if (selectedIndex >= 0)
        {
            preview = preview + unbox_value<hstring>(KeyCombo().SelectedItem());
        }
        HotkeyPreview().Text(preview);

        const bool hasModifier = CtrlCheckBox().IsChecked().Value() ||
            AltCheckBox().IsChecked().Value() || WinCheckBox().IsChecked().Value() ||
            ShiftCheckBox().IsChecked().Value();
        ShortcutWarning().IsOpen(!hasModifier);
    }
}
