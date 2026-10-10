#include "pch.h"
#include "MacroPage.xaml.h"
#include "Localization.h"
#include "MacroEditorState.h"
#include <chrono>

#include <shobjidl_core.h>

#include <algorithm>
#include <cwctype>
#include <memory>
#include <optional>

#if __has_include("MacroPage.g.cpp")
#include "MacroPage.g.cpp"
#endif

using namespace winrt;
using namespace Microsoft::UI::Xaml;

namespace
{
    HWND findApplicationWindow()
    {
        struct WindowSearch
        {
            DWORD processId;
            HWND windowHandle;
        } search{ GetCurrentProcessId(), nullptr };

        EnumWindows([](HWND candidate, LPARAM parameter) -> BOOL
        {
            auto& state = *reinterpret_cast<WindowSearch*>(parameter);
            DWORD candidateProcessId = 0;
            GetWindowThreadProcessId(candidate, &candidateProcessId);
            if (candidateProcessId == state.processId && IsWindowVisible(candidate) &&
                GetWindow(candidate, GW_OWNER) == nullptr)
            {
                state.windowHandle = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&search));
        return search.windowHandle;
    }

    std::optional<std::wstring> showOpenMacroDialog()
    {
        HWND windowHandle = findApplicationWindow();
        if (windowHandle == nullptr)
        {
            throw hresult_error(E_HANDLE, OpenKeyUILocalization::GetString(L"MacroOpenWindowMissing"));
        }

        com_ptr<IFileOpenDialog> dialog;
        check_hresult(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(IFileOpenDialog), dialog.put_void()));
        const auto filterName = OpenKeyUILocalization::GetString(L"MacroFileFilter");
        const COMDLG_FILTERSPEC filters[]{ { filterName.c_str(), L"*.txt" } };
        check_hresult(dialog->SetFileTypes(1, filters));
        check_hresult(dialog->SetDefaultExtension(L"txt"));

        FILEOPENDIALOGOPTIONS options{};
        check_hresult(dialog->GetOptions(&options));
        check_hresult(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST));
        const HRESULT showResult = dialog->Show(windowHandle);
        if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            return std::nullopt;
        }
        check_hresult(showResult);

        com_ptr<IShellItem> item;
        check_hresult(dialog->GetResult(item.put()));
        PWSTR rawPath = nullptr;
        check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath));
        const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(rawPath, &CoTaskMemFree);
        if (!path)
        {
            throw hresult_error(E_UNEXPECTED);
        }
        return std::wstring(path.get());
    }

    std::optional<std::wstring> showSaveMacroDialog()
    {
        HWND windowHandle = findApplicationWindow();
        if (windowHandle == nullptr)
        {
            throw hresult_error(E_HANDLE, OpenKeyUILocalization::GetString(L"MacroSaveWindowMissing"));
        }

        com_ptr<IFileSaveDialog> dialog;
        check_hresult(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER,
            __uuidof(IFileSaveDialog), dialog.put_void()));
        const auto filterName = OpenKeyUILocalization::GetString(L"MacroFileFilter");
        const COMDLG_FILTERSPEC filters[]{ { filterName.c_str(), L"*.txt" } };
        check_hresult(dialog->SetFileTypes(1, filters));
        check_hresult(dialog->SetDefaultExtension(L"txt"));
        check_hresult(dialog->SetFileName(L"OpenKeyMacro.txt"));

        FILEOPENDIALOGOPTIONS options{};
        check_hresult(dialog->GetOptions(&options));
        check_hresult(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_OVERWRITEPROMPT));
        const HRESULT showResult = dialog->Show(windowHandle);
        if (showResult == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            return std::nullopt;
        }
        check_hresult(showResult);

        com_ptr<IShellItem> item;
        check_hresult(dialog->GetResult(item.put()));
        PWSTR rawPath = nullptr;
        check_hresult(item->GetDisplayName(SIGDN_FILESYSPATH, &rawPath));
        const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> path(rawPath, &CoTaskMemFree);
        if (!path)
        {
            throw hresult_error(E_UNEXPECTED);
        }
        return std::wstring(path.get());
    }

    std::wstring lowerCase(std::wstring value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](const wchar_t character)
        {
            return static_cast<wchar_t>(std::towlower(character));
        });
        return value;
    }
}

namespace winrt::OpenKeyUI::implementation
{
    MacroPage::MacroPage()
    {
        InitializeComponent();
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        AutoCapsToggle().IsOn(m_preferences.autoCapsMacro != 0);
        m_loadingPreferences = false;
        const bool loaded = MacroService::tryReadEntries(m_entries);
        RefreshList();
        if (!loaded)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroReloadTitle"),
                OpenKeyUILocalization::GetString(L"MacroStorageError"));
        }
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

    MacroPage::~MacroPage()
    {
        if (m_refreshTimer) m_refreshTimer.Stop();
    }

    MacroPage::BusyScope::BusyScope(MacroPage& page) : m_page(page), m_lease(page.m_operations.TryBegin())
    {
        if (m_lease) m_page.UpdateActionAvailability();
    }

    MacroPage::BusyScope::~BusyScope()
    {
        Finish();
    }

    void MacroPage::BusyScope::Finish() noexcept
    {
        if (!m_lease) return;
        m_lease.Release();
        // Async handlers retain the page through their strong lifetime. A UI
        // failure during cleanup must not terminate a coroutine's unwinding.
        try { m_page.UpdateActionAvailability(); } catch (hresult_error const&) {}
    }

    void MacroPage::UpdateActionAvailability()
    {
        const bool enabled = !m_operations.Busy();
        ReloadButton().IsEnabled(enabled);
        ImportButton().IsEnabled(enabled);
        ExportButton().IsEnabled(enabled);
        NewButton().IsEnabled(enabled);
        SaveButton().IsEnabled(enabled);
        DeleteButton().IsEnabled(enabled && !m_originalShortcut.empty());
        SearchBox().IsEnabled(enabled);
        MacroList().IsEnabled(enabled);
        ShortcutBox().IsEnabled(enabled);
        ReplacementBox().IsEnabled(enabled);
        AutoCapsToggle().IsEnabled(enabled);
    }

    Windows::Foundation::IAsyncOperation<bool> MacroPage::ConfirmDiscardAsync()
    {
        if (!OpenKeyUIMacroEditor::HasUnsavedChanges(m_entries, m_originalShortcut,
            ShortcutBox().Text().c_str(), ReplacementBox().Text().c_str())) co_return true;
        Controls::ContentDialog dialog;
        dialog.XamlRoot(XamlRoot());
        dialog.RequestedTheme(ActualTheme());
        dialog.Title(box_value(OpenKeyUILocalization::GetString(L"MacroDiscardTitle")));
        dialog.Content(box_value(OpenKeyUILocalization::GetString(L"MacroDiscardPrompt")));
        dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"MacroDiscardButton"));
        dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"CancelButtonText"));
        dialog.DefaultButton(Controls::ContentDialogButton::Close);
        co_return co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary;
    }

    void MacroPage::RestoreEditorSelection()
    {
        int index = -1;
        for (size_t i = 0; i < m_visibleEntries.size(); ++i)
            if (m_visibleEntries[i].shortcut == m_originalShortcut) { index = static_cast<int>(i); break; }
        m_refreshingList = true;
        MacroList().SelectedIndex(index);
        m_refreshingList = false;
    }

    void MacroPage::RefreshPreferences()
    {
        if (m_saveFailed || m_operations.Busy()) return;
        m_preferences = SettingsService::readPreferences();
        m_savedPreferences = m_preferences;
        const bool enabled = m_preferences.autoCapsMacro != 0;
        if (AutoCapsToggle().IsOn() == enabled) return;
        m_loadingPreferences = true;
        AutoCapsToggle().IsOn(enabled);
        m_loadingPreferences = false;
    }

    fire_and_forget MacroPage::OnReloadClicked(IInspectable const&, RoutedEventArgs const&)
    {
        [[maybe_unused]] auto lifetime = get_strong();
        BusyScope operation(*this);
        if (!operation) co_return;
        try
        {
            bool reload = true;
            if (OpenKeyUIMacroEditor::HasUnsavedChanges(m_entries, m_originalShortcut,
                ShortcutBox().Text().c_str(), ReplacementBox().Text().c_str()))
            {
                Controls::ContentDialog dialog;
                dialog.XamlRoot(XamlRoot());
                dialog.RequestedTheme(ActualTheme());
                dialog.Title(box_value(OpenKeyUILocalization::GetString(L"MacroReloadTitle")));
                dialog.Content(box_value(OpenKeyUILocalization::GetString(L"MacroReloadPrompt")));
                dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"MacroReloadTitle"));
                dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"CancelButtonText"));
                dialog.DefaultButton(Controls::ContentDialogButton::Close);
                reload = co_await dialog.ShowAsync() == Controls::ContentDialogResult::Primary;
            }
            if (reload)
            {
                if (MacroService::tryReadEntries(m_entries))
                {
                    RefreshList();
                    ClearEditor();
                }
                else
                {
                    ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroReloadTitle"),
                        OpenKeyUILocalization::GetString(L"MacroReloadFailure"));
                }
            }
        }
        catch (hresult_error const& error)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroReloadTitle"), error.message());
        }
    }

    void MacroPage::OnSearchTextChanged(IInspectable const&, Controls::TextChangedEventArgs const&)
    {
        if (m_operations.Busy()) return;
        RefreshList();
    }

    void MacroPage::RefreshList()
    {
        m_refreshingList = true;
        MacroList().Items().Clear();
        m_visibleEntries.clear();
        const std::wstring query = lowerCase(SearchBox().Text().c_str());
        int visibleCount = 0;

        for (const auto& entry : m_entries)
        {
            const bool matches = query.empty() ||
                lowerCase(entry.shortcut).find(query) != std::wstring::npos ||
                lowerCase(entry.replacement).find(query) != std::wstring::npos;
            if (!matches)
            {
                continue;
            }

            MacroList().Items().Append(box_value(hstring(entry.shortcut + L"  →  " + entry.replacement)));
            m_visibleEntries.push_back(entry);
            ++visibleCount;
        }

        MacroCount().Text(visibleCount == 1
            ? OpenKeyUILocalization::GetString(L"MacroCountOne")
            : OpenKeyUILocalization::Format(L"MacroCountFormat", to_hstring(visibleCount)));
        EmptyState().Visibility(visibleCount == 0 ? Visibility::Visible : Visibility::Collapsed);
        MacroList().Visibility(visibleCount == 0 ? Visibility::Collapsed : Visibility::Visible);
        m_refreshingList = false;
    }

    fire_and_forget MacroPage::OnMacroSelectionChanged(IInspectable const&, Controls::SelectionChangedEventArgs const&)
    {
        [[maybe_unused]] auto lifetime = get_strong();
        if (m_refreshingList || m_operations.Busy() || MacroList().SelectedIndex() < 0)
        {
            co_return;
        }
        const auto index = static_cast<size_t>(MacroList().SelectedIndex());
        if (index >= m_visibleEntries.size()) co_return;
        const auto entry = m_visibleEntries[index];
        BusyScope operation(*this);
        if (!operation) co_return;
        try
        {
            if (!co_await ConfirmDiscardAsync()) { RestoreEditorSelection(); co_return; }
            m_originalShortcut = entry.shortcut;
            ShortcutBox().Text(entry.shortcut);
            ReplacementBox().Text(entry.replacement);
            EditorTitle().Text(OpenKeyUILocalization::GetString(L"MacroEditTitle"));
            SaveButton().Content(box_value(OpenKeyUILocalization::GetString(L"MacroSaveChanges")));
            EditorStatus().IsOpen(false);
        }
        catch (hresult_error const& error)
        {
            RestoreEditorSelection();
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroDiscardTitle"), error.message());
        }
    }

    fire_and_forget MacroPage::OnNewClicked(IInspectable const&, RoutedEventArgs const&)
    {
        [[maybe_unused]] auto lifetime = get_strong();
        BusyScope operation(*this);
        if (!operation) co_return;
        try
        {
            if (!co_await ConfirmDiscardAsync()) co_return;
            ClearEditor();
            operation.Finish();
            ShortcutBox().Focus(FocusState::Programmatic);
        }
        catch (hresult_error const& error)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroDiscardTitle"), error.message());
        }
    }

    void MacroPage::OnSaveClicked(IInspectable const&, RoutedEventArgs const&)
    {
        BusyScope operation(*this);
        if (!operation) return;
        const MacroService::Entry entry{ ShortcutBox().Text().c_str(), ReplacementBox().Text().c_str() };
        const auto result = MacroService::saveEntry(entry, m_originalShortcut);
        if (result != MacroService::SaveResult::Success)
        {
            hstring message;
            switch (result)
            {
            case MacroService::SaveResult::EmptyShortcut:
                message = OpenKeyUILocalization::GetString(L"MacroEmptyShortcut");
                break;
            case MacroService::SaveResult::EmptyReplacement:
                message = OpenKeyUILocalization::GetString(L"MacroEmptyReplacement");
                break;
            case MacroService::SaveResult::ShortcutTooLong:
                message = OpenKeyUILocalization::GetString(L"MacroInvalidShortcut");
                break;
            case MacroService::SaveResult::ReplacementTooLong:
                message = OpenKeyUILocalization::GetString(L"MacroInvalidReplacement");
                break;
            case MacroService::SaveResult::DuplicateShortcut:
                message = OpenKeyUILocalization::GetString(L"MacroDuplicateShortcut");
                break;
            default:
                message = OpenKeyUILocalization::GetString(L"MacroStorageError");
                break;
            }
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroSaveFailureTitle"), message);
            return;
        }

        if (!ReloadAfterWrite()) return;
        RefreshList();
        ClearEditor();
        ShowStatus(true, OpenKeyUILocalization::GetString(L"MacroSaveSuccessTitle"), OpenKeyUILocalization::GetString(L"MacroSaveSuccessMessage"));
        operation.Finish();
        ShortcutBox().Focus(FocusState::Keyboard);
    }

    void MacroPage::OnDeleteClicked(IInspectable const&, RoutedEventArgs const&)
    {
        DeleteSelectedAsync();
    }

    fire_and_forget MacroPage::DeleteSelectedAsync()
    {
        [[maybe_unused]] auto lifetime = get_strong();
        BusyScope operation(*this);
        if (!operation) co_return;
        if (m_originalShortcut.empty())
        {
            co_return;
        }

        const auto shortcutToDelete = m_originalShortcut;
        try
        {
            Controls::ContentDialog dialog;
            dialog.XamlRoot(XamlRoot());
            dialog.RequestedTheme(ActualTheme());
            dialog.Title(box_value(OpenKeyUILocalization::GetString(L"MacroDeleteConfirmTitle")));
            dialog.Content(box_value(OpenKeyUILocalization::Format(L"MacroDeleteConfirmFormat", hstring(shortcutToDelete))));
            dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"DeleteButtonText"));
            dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"CancelButtonText"));
            dialog.DefaultButton(Controls::ContentDialogButton::Close);

            if (co_await dialog.ShowAsync() != Controls::ContentDialogResult::Primary)
            {
                co_return;
            }

            const bool deleted = MacroService::deleteEntry(shortcutToDelete);
            if (deleted)
            {
                if (!ReloadAfterWrite()) co_return;
                RefreshList();
                ClearEditor();
            }
            ShowStatus(deleted,
                deleted ? OpenKeyUILocalization::GetString(L"MacroDeleteSuccessTitle") : OpenKeyUILocalization::GetString(L"MacroDeleteFailureTitle"),
                deleted ? OpenKeyUILocalization::GetString(L"MacroDeleteSuccessMessage") : OpenKeyUILocalization::GetString(L"MacroDeleteFailureMessage"));
            if (deleted)
            {
                operation.Finish();
                if (m_entries.empty())
                {
                    ShortcutBox().Focus(FocusState::Keyboard);
                }
                else
                {
                    MacroList().Focus(FocusState::Keyboard);
                }
            }
        }
        catch (hresult_error const& error)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroDeleteFailureTitle"), error.message());
        }
    }

    fire_and_forget MacroPage::OnImportClicked(IInspectable const&, RoutedEventArgs const&)
    {
        [[maybe_unused]] auto lifetime = get_strong();
        BusyScope operation(*this);
        if (!operation) co_return;
        try
        {
            if (!co_await ConfirmDiscardAsync()) co_return;
            const auto path = showOpenMacroDialog();
            if (!path)
            {
                co_return;
            }

            Controls::ContentDialog dialog;
            dialog.XamlRoot(XamlRoot());
            dialog.RequestedTheme(ActualTheme());
            dialog.Title(box_value(OpenKeyUILocalization::GetString(L"MacroImportTitle")));
            dialog.Content(box_value(
                OpenKeyUILocalization::GetString(L"MacroImportPrompt")));
            dialog.PrimaryButtonText(OpenKeyUILocalization::GetString(L"MacroImportMerge"));
            dialog.SecondaryButtonText(OpenKeyUILocalization::GetString(L"MacroImportReplace"));
            dialog.CloseButtonText(OpenKeyUILocalization::GetString(L"CancelButtonText"));
            dialog.DefaultButton(Controls::ContentDialogButton::Close);

            const auto choice = co_await dialog.ShowAsync();
            if (choice == Controls::ContentDialogResult::None)
            {
                co_return;
            }

            const auto result = MacroService::importFromFile(
                *path, choice == Controls::ContentDialogResult::Primary);
            if (result.status == MacroService::ImportStatus::Success)
            {
                if (!ReloadAfterWrite()) co_return;
                SearchBox().Text(L"");
                RefreshList();
                ClearEditor();

                hstring message = OpenKeyUILocalization::Format(L"MacroImportedFormat", to_hstring(result.importedCount));
                if (result.skippedCount > 0)
                {
                    message = message + OpenKeyUILocalization::Format(L"MacroSkippedFormat", to_hstring(result.skippedCount));
                }
                ShowStatus(true, OpenKeyUILocalization::GetString(L"MacroImportSuccessTitle"), message);
                operation.Finish();
                if (m_entries.empty())
                {
                    ShortcutBox().Focus(FocusState::Keyboard);
                }
                else
                {
                    MacroList().Focus(FocusState::Keyboard);
                }
                co_return;
            }

            hstring message;
            switch (result.status)
            {
            case MacroService::ImportStatus::InvalidFormat:
                message = OpenKeyUILocalization::GetString(L"MacroImportInvalidFormat");
                break;
            case MacroService::ImportStatus::NoValidEntries:
                message = OpenKeyUILocalization::GetString(L"MacroImportEmpty");
                break;
            case MacroService::ImportStatus::StorageError:
                message = OpenKeyUILocalization::GetString(L"MacroImportStorageError");
                break;
            default:
                message = OpenKeyUILocalization::GetString(L"MacroReadFailure");
                break;
            }
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroImportFailureTitle"), message);
        }
        catch (hresult_error const& error)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroOpenFailureTitle"), error.message());
        }
    }

    void MacroPage::OnExportClicked(IInspectable const&, RoutedEventArgs const&)
    {
        [[maybe_unused]] auto lifetime = get_strong();
        BusyScope operation(*this);
        if (!operation) return;
        try
        {
            const auto path = showSaveMacroDialog();
            if (!path)
            {
                return;
            }

            const auto result = MacroService::exportToFile(*path);
            switch (result)
            {
            case MacroService::ExportResult::Success:
                ShowStatus(true, OpenKeyUILocalization::GetString(L"MacroExportSuccessTitle"),
                    OpenKeyUILocalization::GetString(L"MacroExportSuccessMessage"));
                break;
            case MacroService::ExportResult::UnsupportedMultilineContent:
                ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroExportFailureTitle"),
                    OpenKeyUILocalization::GetString(L"MacroExportMultiline"));
                break;
            case MacroService::ExportResult::StorageError:
                ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroExportFailureTitle"),
                    OpenKeyUILocalization::GetString(L"MacroStorageError"));
                break;
            default:
                ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroExportFailureTitle"), OpenKeyUILocalization::GetString(L"MacroWriteFailure"));
                break;
            }
        }
        catch (hresult_error const& error)
        {
            ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroSaveFileFailureTitle"), error.message());
        }
    }

    void MacroPage::OnAutoCapsToggled(IInspectable const&, RoutedEventArgs const&)
    {
        if (m_loadingPreferences)
        {
            return;
        }

        m_preferences.autoCapsMacro = AutoCapsToggle().IsOn() ? 1 : 0;
        const bool saved = SettingsService::saveChangedPreferences(m_savedPreferences, m_preferences);
        m_saveFailed = !saved;
        ShowStatus(saved,
            saved ? OpenKeyUILocalization::GetString(L"MacroOptionsSuccessTitle") : OpenKeyUILocalization::GetString(L"MacroOptionsFailureTitle"),
            saved
                ? OpenKeyUILocalization::GetString(L"MacroAutoCapsMessage")
                : OpenKeyUILocalization::GetString(L"SaveFailureMessage"));
    }

    void MacroPage::OnFocusSearchInvoked(
        Input::KeyboardAccelerator const&, Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        if (!m_operations.Busy())
        {
            SearchBox().Focus(FocusState::Keyboard);
            SearchBox().SelectAll();
        }
        args.Handled(true);
    }

    void MacroPage::OnDeleteAcceleratorInvoked(
        Input::KeyboardAccelerator const&, Input::KeyboardAcceleratorInvokedEventArgs const& args)
    {
        if (DeleteButton().IsEnabled())
        {
            DeleteSelectedAsync();
            args.Handled(true);
        }
    }

    bool MacroPage::ReloadAfterWrite()
    {
        if (MacroService::tryReadEntries(m_entries)) return true;
        ShowStatus(false, OpenKeyUILocalization::GetString(L"MacroRefreshAfterWriteTitle"),
            OpenKeyUILocalization::GetString(L"MacroRefreshAfterWriteFailure"));
        EditorStatus().Severity(Controls::InfoBarSeverity::Warning);
        return false;
    }

    void MacroPage::ClearEditor()
    {
        m_originalShortcut.clear();
        MacroList().SelectedIndex(-1);
        ShortcutBox().Text(L"");
        ReplacementBox().Text(L"");
        EditorTitle().Text(OpenKeyUILocalization::GetString(L"MacroAddTitle"));
        SaveButton().Content(box_value(OpenKeyUILocalization::GetString(L"AddButtonText")));
        DeleteButton().IsEnabled(false);
        EditorStatus().IsOpen(false);
    }

    void MacroPage::ShowStatus(const bool success, hstring const& title, hstring const& message)
    {
        EditorStatus().Severity(success
            ? Controls::InfoBarSeverity::Success
            : Controls::InfoBarSeverity::Error);
        EditorStatus().Title(title);
        EditorStatus().Message(message);
        EditorStatus().IsOpen(true);
    }
}
