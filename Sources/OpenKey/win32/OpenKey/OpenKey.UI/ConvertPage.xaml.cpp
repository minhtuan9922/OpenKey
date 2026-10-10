#include "pch.h"
#include "ConvertPage.xaml.h"
#include "Localization.h"

#include <algorithm>
#include <array>
#include <chrono>

#if __has_include("ConvertPage.g.cpp")
#include "ConvertPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    constexpr int ControlMask = 0x100;
    constexpr int AltMask = 0x200;
    constexpr int WindowsMask = 0x400;
    constexpr int ShiftMask = 0x800;
    constexpr int EmptyKey = 0xFE;

    struct HotkeyChoice
    {
        wchar_t const* label;
        int code;
    };

    constexpr std::array<HotkeyChoice, 49> HotkeyChoices{{
        { L"", EmptyKey }, { L"Space", VK_SPACE },
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
    ConvertPage::ConvertPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        for (const auto& choice : HotkeyChoices)
        {
            QuickKeyCombo().Items().Append(box_value(choice.code == EmptyKey
                ? OpenKeyUILocalization::GetString(L"QuickShortcutDisabled")
                : hstring(choice.label)));
            m_quickKeyCodes.push_back(choice.code);
        }
        ApplyPreferences();
        m_loadingPreferences = false;
        UpdateActionState();
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

    ConvertPage::~ConvertPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    void ConvertPage::RefreshPreferences()
    {
        if (m_saveFailed || SourceCodeCombo().IsDropDownOpen() ||
            TargetCodeCombo().IsDropDownOpen() || QuickKeyCombo().IsDropDownOpen()) return;
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        m_loadingPreferences = true;
        ApplyPreferences();
        m_loadingPreferences = false;
    }

    void ConvertPage::ApplyPreferences()
    {
        const auto select = [](Controls::ComboBox const& combo, const int index)
        {
            if (combo.SelectedIndex() != index) combo.SelectedIndex(index);
        };
        const auto check = [](Controls::CheckBox const& box, const int value)
        {
            if (box.IsChecked().GetBoolean() != (value != 0)) box.IsChecked(value != 0);
        };
        select(SourceCodeCombo(), std::clamp(m_preferences.convertToolFromCode, 0, 4));
        select(TargetCodeCombo(), std::clamp(m_preferences.convertToolToCode, 0, 4));
        check(AllCapsCheck(), m_preferences.convertToolToAllCaps);
        check(AllLowerCheck(), m_preferences.convertToolToAllNonCaps);
        check(SentenceCapsCheck(), m_preferences.convertToolToCapsFirstLetter);
        check(WordCapsCheck(), m_preferences.convertToolToCapsEachWord);
        check(RemoveMarkCheck(), m_preferences.convertToolRemoveMark);
        const bool notify = m_preferences.convertToolDontAlertWhenCompleted == 0;
        if (NotifyToggle().IsOn() != notify) NotifyToggle().IsOn(notify);

        const int hotkey = m_preferences.convertToolHotKey;
        QuickCtrlCheck().IsChecked((hotkey & ControlMask) != 0);
        QuickAltCheck().IsChecked((hotkey & AltMask) != 0);
        QuickWinCheck().IsChecked((hotkey & WindowsMask) != 0);
        QuickShiftCheck().IsChecked((hotkey & ShiftMask) != 0);
        const int currentKey = hotkey & 0xFF;
        const auto selected = std::find(m_quickKeyCodes.begin(), m_quickKeyCodes.end(), currentKey);
        if (selected == m_quickKeyCodes.end())
        {
            QuickKeyCombo().Items().Append(box_value(OpenKeyUILocalization::GetString(L"CurrentKeyPrefix") + to_hstring(currentKey) + L")"));
            m_quickKeyCodes.push_back(currentKey);
            select(QuickKeyCombo(), static_cast<int>(m_quickKeyCodes.size() - 1));
        }
        else
        {
            select(QuickKeyCombo(), static_cast<int>(std::distance(m_quickKeyCodes.begin(), selected)));
        }
        UpdateHotkeyPreview();
    }

    void ConvertPage::OnCodeTableChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        if (m_loadingPreferences || SourceCodeCombo().SelectedIndex() < 0 || TargetCodeCombo().SelectedIndex() < 0)
        {
            return;
        }
        m_preferences.convertToolFromCode = SourceCodeCombo().SelectedIndex();
        m_preferences.convertToolToCode = TargetCodeCombo().SelectedIndex();
        SavePreferences();
    }

    void ConvertPage::OnSwapClicked(IInspectable const&, RoutedEventArgs const&)
    {
        m_loadingPreferences = true;
        const int source = SourceCodeCombo().SelectedIndex();
        SourceCodeCombo().SelectedIndex(TargetCodeCombo().SelectedIndex());
        TargetCodeCombo().SelectedIndex(source);
        m_loadingPreferences = false;
        m_preferences.convertToolFromCode = SourceCodeCombo().SelectedIndex();
        m_preferences.convertToolToCode = TargetCodeCombo().SelectedIndex();
        SavePreferences();
        StatusBar().IsOpen(false);
    }

    void ConvertPage::OnOptionChanged(IInspectable const& sender, RoutedEventArgs const&)
    {
        if (m_loadingPreferences)
        {
            return;
        }

        const auto selected = sender.try_as<Controls::CheckBox>();
        // Removing marks is independent of the mutually exclusive case transformations.
        if (selected && selected != RemoveMarkCheck() && selected.IsChecked().GetBoolean())
        {
            m_loadingPreferences = true;
            for (const auto& option : { AllCapsCheck(), AllLowerCheck(), SentenceCapsCheck(), WordCapsCheck() })
            {
                if (option != selected)
                {
                    option.IsChecked(false);
                }
            }
            m_loadingPreferences = false;
        }

        m_preferences.convertToolToAllCaps = AllCapsCheck().IsChecked().GetBoolean() ? 1 : 0;
        m_preferences.convertToolToAllNonCaps = AllLowerCheck().IsChecked().GetBoolean() ? 1 : 0;
        m_preferences.convertToolToCapsFirstLetter = SentenceCapsCheck().IsChecked().GetBoolean() ? 1 : 0;
        m_preferences.convertToolToCapsEachWord = WordCapsCheck().IsChecked().GetBoolean() ? 1 : 0;
        m_preferences.convertToolRemoveMark = RemoveMarkCheck().IsChecked().GetBoolean() ? 1 : 0;
        SavePreferences();
    }

    void ConvertPage::OnInputTextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        StatusBar().IsOpen(false);
        UpdateActionState();
    }

    void ConvertPage::OnConvertClicked(IInspectable const&, RoutedEventArgs const&)
    {
        if (InputBox().Text().empty())
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"ConvertEmptyTitle"), OpenKeyUILocalization::GetString(L"ConvertEmptyMessage"));
            InputBox().Focus(FocusState::Keyboard);
            return;
        }

        ResultBox().Text(ConvertService::convert(InputBox().Text().c_str(), CurrentOptions()));
        UpdateActionState();
        ShowStatus(true, OpenKeyUILocalization::GetString(L"ConvertSuccessTitle"), OpenKeyUILocalization::GetString(L"ConvertSuccessMessage"));
    }

    void ConvertPage::OnCopyClicked(IInspectable const&, RoutedEventArgs const&)
    {
        if (ResultBox().Text().empty())
        {
            return;
        }
        Windows::ApplicationModel::DataTransfer::DataPackage package;
        package.SetText(ResultBox().Text());
        Windows::ApplicationModel::DataTransfer::Clipboard::SetContent(package);
        Windows::ApplicationModel::DataTransfer::Clipboard::Flush();
        ShowStatus(true, OpenKeyUILocalization::GetString(L"CopySuccessTitle"), OpenKeyUILocalization::GetString(L"CopySuccessMessage"));
    }

    void ConvertPage::OnClearClicked(IInspectable const&, RoutedEventArgs const&)
    {
        InputBox().Text(L"");
        ResultBox().Text(L"");
        StatusBar().IsOpen(false);
        UpdateActionState();
        InputBox().Focus(FocusState::Keyboard);
    }

    void ConvertPage::OnQuickConvertClicked(IInspectable const&, RoutedEventArgs const&)
    {
        switch (ConvertService::convertClipboard(CurrentOptions()))
        {
        case ConvertService::ClipboardResult::Success:
            ShowStatus(true, OpenKeyUILocalization::GetString(L"ClipboardConvertSuccessTitle"),
                OpenKeyUILocalization::GetString(L"ClipboardConvertSuccessMessage"));
            break;
        case ConvertService::ClipboardResult::NoText:
            ShowStatus(false, OpenKeyUILocalization::GetString(L"ClipboardEmptyTitle"),
                OpenKeyUILocalization::GetString(L"ClipboardEmptyMessage"));
            break;
        case ConvertService::ClipboardResult::ClipboardUnavailable:
            ShowStatus(false, OpenKeyUILocalization::GetString(L"ClipboardBusyTitle"),
                OpenKeyUILocalization::GetString(L"ClipboardBusyMessage"));
            break;
        default:
            ShowStatus(false, OpenKeyUILocalization::GetString(L"ClipboardWriteFailureTitle"),
                OpenKeyUILocalization::GetString(L"ClipboardWriteFailureMessage"));
            break;
        }
    }

    void ConvertPage::OnQuickHotkeyChanged(IInspectable const&, RoutedEventArgs const&)
    {
        SaveQuickHotkey();
    }

    void ConvertPage::OnQuickKeyChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        SaveQuickHotkey();
    }

    void ConvertPage::SaveQuickHotkey()
    {
        if (m_loadingPreferences || QuickKeyCombo().SelectedIndex() < 0)
        {
            return;
        }
        const int keyCode = m_quickKeyCodes.at(static_cast<size_t>(QuickKeyCombo().SelectedIndex()));
        int hotkey = keyCode | (keyCode << 24);
        hotkey |= QuickCtrlCheck().IsChecked().GetBoolean() ? ControlMask : 0;
        hotkey |= QuickAltCheck().IsChecked().GetBoolean() ? AltMask : 0;
        hotkey |= QuickWinCheck().IsChecked().GetBoolean() ? WindowsMask : 0;
        hotkey |= QuickShiftCheck().IsChecked().GetBoolean() ? ShiftMask : 0;
        m_preferences.convertToolHotKey = hotkey;
        SavePreferences();
        UpdateHotkeyPreview();
    }

    void ConvertPage::OnNotifyToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences)
        {
            return;
        }
        m_preferences.convertToolDontAlertWhenCompleted = NotifyToggle().IsOn() ? 0 : 1;
        SavePreferences();
    }

    void ConvertPage::SavePreferences()
    {
        if (m_loadingPreferences) return;
        m_saveFailed = !SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        if (m_saveFailed)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"SaveOptionsFailureTitle"), OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
        }
    }

    void ConvertPage::UpdateActionState()
    {
        ConvertButton().IsEnabled(!InputBox().Text().empty());
        CopyButton().IsEnabled(!ResultBox().Text().empty());
        ClearButton().IsEnabled(!InputBox().Text().empty() || !ResultBox().Text().empty());
    }

    void ConvertPage::UpdateHotkeyPreview()
    {
        hstring preview;
        if (QuickCtrlCheck().IsChecked().GetBoolean()) preview = preview + L"Ctrl + ";
        if (QuickAltCheck().IsChecked().GetBoolean()) preview = preview + L"Alt + ";
        if (QuickWinCheck().IsChecked().GetBoolean()) preview = preview + L"Win + ";
        if (QuickShiftCheck().IsChecked().GetBoolean()) preview = preview + L"Shift + ";
        if (QuickKeyCombo().SelectedIndex() >= 0)
        {
            preview = preview + unbox_value<hstring>(QuickKeyCombo().SelectedItem());
        }
        QuickHotkeyPreview().Text(preview);

        const bool enabled = QuickKeyCombo().SelectedIndex() > 0;
        const bool hasModifier = QuickCtrlCheck().IsChecked().GetBoolean() ||
            QuickAltCheck().IsChecked().GetBoolean() || QuickWinCheck().IsChecked().GetBoolean() ||
            QuickShiftCheck().IsChecked().GetBoolean();
        QuickHotkeyWarning().IsOpen(enabled && !hasModifier);
    }

    ConvertService::Options ConvertPage::CurrentOptions()
    {
        return {
            SourceCodeCombo().SelectedIndex(),
            TargetCodeCombo().SelectedIndex(),
            AllCapsCheck().IsChecked().GetBoolean(),
            AllLowerCheck().IsChecked().GetBoolean(),
            SentenceCapsCheck().IsChecked().GetBoolean(),
            WordCapsCheck().IsChecked().GetBoolean(),
            RemoveMarkCheck().IsChecked().GetBoolean()
        };
    }

    void ConvertPage::ShowStatus(const bool success, hstring const& title, hstring const& message)
    {
        StatusBar().Severity(success ? Controls::InfoBarSeverity::Success : Controls::InfoBarSeverity::Error);
        StatusBar().Title(title);
        StatusBar().Message(message);
        StatusBar().IsOpen(true);
    }
}
