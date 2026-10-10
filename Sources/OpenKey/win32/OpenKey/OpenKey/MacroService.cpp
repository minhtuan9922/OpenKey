#include <windows.h>

#include "MacroService.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

namespace {
	constexpr wchar_t RegistryPath[] = L"SOFTWARE\\TuyenMai\\OpenKey";
	constexpr wchar_t MacroDataValue[] = L"macroData";
	constexpr char MacroFileHeader[] = ";Compatible OpenKey Macro Data file for UniKey*** version=1 ***";

	// Own only the newly created sibling, never the destination or another writer's file.
	struct ExportTemporaryFile {
		std::wstring path;
		HANDLE handle = INVALID_HANDLE_VALUE;
		bool owned = false;
		~ExportTemporaryFile() {
			if (handle != INVALID_HANDLE_VALUE) CloseHandle(handle);
			if (owned) DeleteFileW(path.c_str());
		}
		bool close() {
			if (!CloseHandle(handle)) return false;
			handle = INVALID_HANDLE_VALUE;
			return true;
		}
	};

	bool writeExport(HANDLE handle, const std::string& text) {
		size_t offset = 0;
		while (offset < text.size()) {
			const DWORD count = static_cast<DWORD>((std::min)(text.size() - offset, size_t{65536}));
			DWORD written = 0;
			if (!WriteFile(handle, text.data() + offset, count, &written, nullptr) ||
				written == 0 || written > count) return false;
			offset += written;
		}
		return true;
	}

	bool isValidUtf8(const std::string& value) {
		return value.empty() || MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			value.data(), static_cast<int>(value.size()), nullptr, 0) > 0;
	}

	std::wstring utf8ToWide(const std::string& value) {
		if (value.empty()) {
			return {};
		}

		int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
			value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (length == 0) {
			length = MultiByteToWideChar(CP_UTF8, 0,
				value.data(), static_cast<int>(value.size()), nullptr, 0);
		}
		if (length == 0) {
			return {};
		}

		std::wstring converted(static_cast<size_t>(length), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
			converted.data(), length);
		return converted;
	}

	std::string wideToUtf8(const std::wstring& value) {
		if (value.empty()) {
			return {};
		}

		const int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
			value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (length == 0) {
			return {};
		}

		std::string converted(static_cast<size_t>(length), '\0');
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), converted.data(), length, nullptr, nullptr);
		return converted;
	}

	bool readMacroData(std::vector<std::uint8_t>& data, const bool missingIsEmpty = false) {
		HKEY key = nullptr;
		const LONG opened = RegOpenKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, KEY_READ, &key);
		if (opened != ERROR_SUCCESS) {
			if (missingIsEmpty && opened == ERROR_FILE_NOT_FOUND) {
				data = { 0, 0 };
				return true;
			}
			return false;
		}

		DWORD type = 0;
		DWORD size = 0;
		LONG result = RegQueryValueExW(key, MacroDataValue, nullptr, &type, nullptr, &size);
		if (missingIsEmpty && result == ERROR_FILE_NOT_FOUND) {
			RegCloseKey(key);
			data = { 0, 0 };
			return true;
		}
		if (result != ERROR_SUCCESS || type != REG_BINARY || size == 0) {
			RegCloseKey(key);
			return false;
		}

		data.resize(size);
		result = RegQueryValueExW(key, MacroDataValue, nullptr, &type,
			reinterpret_cast<LPBYTE>(data.data()), &size);
		RegCloseKey(key);
		if (result != ERROR_SUCCESS || type != REG_BINARY || size > data.size()) return false;
		data.resize(size);
		return true;
	}

	bool writeEntries(const std::vector<MacroService::Entry>& entries) {
		if (entries.size() > UINT16_MAX) {
			return false;
		}

		std::vector<std::uint8_t> data;
		data.reserve(2 + (entries.size() * 8));
		const auto count = static_cast<std::uint16_t>(entries.size());
		data.push_back(static_cast<std::uint8_t>(count));
		data.push_back(static_cast<std::uint8_t>(count >> 8));

		for (const auto& entry : entries) {
			const std::string shortcut = wideToUtf8(entry.shortcut);
			const std::string replacement = wideToUtf8(entry.replacement);
			if (shortcut.empty() || shortcut.size() > UINT8_MAX ||
				(!entry.replacement.empty() && replacement.empty()) || replacement.size() > UINT16_MAX) {
				return false;
			}

			data.push_back(static_cast<std::uint8_t>(shortcut.size()));
			data.insert(data.end(), shortcut.begin(), shortcut.end());
			const auto replacementSize = static_cast<std::uint16_t>(replacement.size());
			data.push_back(static_cast<std::uint8_t>(replacementSize));
			data.push_back(static_cast<std::uint8_t>(replacementSize >> 8));
			data.insert(data.end(), replacement.begin(), replacement.end());
		}

		HKEY key = nullptr;
		DWORD disposition = 0;
		if (RegCreateKeyExW(HKEY_CURRENT_USER, RegistryPath, 0, nullptr,
			REG_OPTION_NON_VOLATILE, KEY_SET_VALUE, nullptr, &key, &disposition) != ERROR_SUCCESS) {
			return false;
		}

		const LONG result = RegSetValueExW(key, MacroDataValue, 0, REG_BINARY,
			reinterpret_cast<const BYTE*>(data.data()), static_cast<DWORD>(data.size()));
		RegCloseKey(key);
		return result == ERROR_SUCCESS;
	}
}

std::vector<MacroService::Entry> MacroService::readEntries() {
	std::vector<std::uint8_t> data;
	if (!readMacroData(data) || data.size() < 2) {
		return {};
	}

	const std::uint16_t count = static_cast<std::uint16_t>(data[0]) |
		(static_cast<std::uint16_t>(data[1]) << 8);
	size_t cursor = 2;
	std::vector<Entry> entries;
	entries.reserve(count);

	for (std::uint16_t index = 0; index < count; ++index) {
		if (cursor >= data.size()) {
			break;
		}

		const size_t shortcutSize = data[cursor++];
		if (shortcutSize > data.size() - cursor) {
			break;
		}
		const std::string shortcut(reinterpret_cast<const char*>(data.data() + cursor), shortcutSize);
		cursor += shortcutSize;

		if (data.size() - cursor < 2) {
			break;
		}
		const size_t replacementSize = static_cast<std::uint16_t>(data[cursor]) |
			(static_cast<std::uint16_t>(data[cursor + 1]) << 8);
		cursor += 2;
		if (replacementSize > data.size() - cursor) {
			break;
		}
		const std::string replacement(
			reinterpret_cast<const char*>(data.data() + cursor), replacementSize);
		cursor += replacementSize;

		entries.push_back({ utf8ToWide(shortcut), utf8ToWide(replacement) });
	}

	return entries;
}

bool MacroService::tryReadEntries(std::vector<Entry>& entries) {
	std::vector<std::uint8_t> data;
	if (!readMacroData(data, true) || data.size() < 2) return false;
	const size_t count = static_cast<std::uint16_t>(data[0]) |
		(static_cast<std::uint16_t>(data[1]) << 8);
	if (count > (data.size() - 2) / 4) return false;
	size_t cursor = 2;
	std::vector<Entry> candidate;
	candidate.reserve(count);
	for (size_t index = 0; index < count; ++index) {
		if (cursor >= data.size()) return false;
		const size_t shortcutSize = data[cursor++];
		if (shortcutSize == 0 || shortcutSize > data.size() - cursor) return false;
		const std::string shortcut(reinterpret_cast<const char*>(data.data() + cursor), shortcutSize);
		cursor += shortcutSize;
		if (data.size() - cursor < 2) return false;
		const size_t replacementSize = static_cast<std::uint16_t>(data[cursor]) |
			(static_cast<std::uint16_t>(data[cursor + 1]) << 8);
		cursor += 2;
		if (replacementSize > data.size() - cursor) return false;
		const std::string replacement(reinterpret_cast<const char*>(data.data() + cursor), replacementSize);
		cursor += replacementSize;
		if (!isValidUtf8(shortcut) || !isValidUtf8(replacement)) return false;
		candidate.push_back({ utf8ToWide(shortcut), utf8ToWide(replacement) });
	}
	if (cursor != data.size()) return false;
	entries = std::move(candidate);
	return true;
}

MacroService::SaveResult MacroService::saveEntry(
	const Entry& entry, const std::wstring& originalShortcut) {
	if (entry.shortcut.empty()) {
		return SaveResult::EmptyShortcut;
	}
	if (entry.replacement.empty()) {
		return SaveResult::EmptyReplacement;
	}

	const std::string shortcut = wideToUtf8(entry.shortcut);
	const std::string replacement = wideToUtf8(entry.replacement);
	if (shortcut.empty() || shortcut.size() > UINT8_MAX) {
		return SaveResult::ShortcutTooLong;
	}
	if (replacement.empty() || replacement.size() > UINT16_MAX) {
		return SaveResult::ReplacementTooLong;
	}

		std::vector<Entry> entries;
		if (!tryReadEntries(entries)) return SaveResult::StorageError;
		auto original = std::find_if(entries.begin(), entries.end(), [&](const Entry& existing) {
		return existing.shortcut == originalShortcut;
	});
	auto duplicate = std::find_if(entries.begin(), entries.end(), [&](const Entry& existing) {
		return existing.shortcut == entry.shortcut;
	});
	if (duplicate != entries.end() && duplicate != original) {
		return SaveResult::DuplicateShortcut;
	}

	if (original != entries.end()) {
		*original = entry;
	}
	else {
		entries.push_back(entry);
	}

	return writeEntries(entries) ? SaveResult::Success : SaveResult::StorageError;
}

bool MacroService::deleteEntry(const std::wstring& shortcut) {
		std::vector<Entry> entries;
		if (!tryReadEntries(entries)) return false;
	const auto entry = std::find_if(entries.begin(), entries.end(), [&](const Entry& existing) {
		return existing.shortcut == shortcut;
	});
	if (entry == entries.end()) {
		return false;
	}

	entries.erase(entry);
	return writeEntries(entries);
}

MacroService::ImportResult MacroService::importFromFile(
	const std::wstring& path, const bool keepExisting) {
	std::ifstream input(std::filesystem::path(path), std::ios::binary);
	if (!input) {
		return { ImportStatus::FileError, 0, 0 };
	}

	std::string header;
	if (!std::getline(input, header)) {
		return { ImportStatus::InvalidFormat, 0, 0 };
	}
	if (!header.empty() && header.back() == '\r') {
		header.pop_back();
	}
	if (header.starts_with("\xEF\xBB\xBF")) {
		header.erase(0, 3);
	}
	if (header != MacroFileHeader) {
		return { ImportStatus::InvalidFormat, 0, 0 };
	}

		std::vector<Entry> entries;
		// Explicit replacement can recover a malformed store; merging must preserve it.
		if (keepExisting && !tryReadEntries(entries)) {
			return { ImportStatus::StorageError, 0, 0 };
		}
	size_t importedCount = 0;
	size_t skippedCount = 0;
	std::string line;
	while (std::getline(input, line)) {
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}

		const size_t separator = line.find(':');
		if (separator == std::string::npos) {
			if (!line.empty()) {
				++skippedCount;
			}
			continue;
		}

		std::string shortcutUtf8 = line.substr(0, separator);
		std::string replacementUtf8 = line.substr(separator + 1);
		while (shortcutUtf8.empty() && !replacementUtf8.empty()) {
			const size_t nextSeparator = replacementUtf8.find(':');
			if (nextSeparator == std::string::npos) {
				break;
			}
			shortcutUtf8 = ":" + replacementUtf8.substr(0, nextSeparator);
			replacementUtf8.erase(0, nextSeparator + 1);
		}
		const std::wstring shortcut = utf8ToWide(shortcutUtf8);
		const std::wstring replacement = utf8ToWide(replacementUtf8);
		if (shortcutUtf8.size() > UINT8_MAX || replacementUtf8.size() > UINT16_MAX ||
			!isValidUtf8(shortcutUtf8) || !isValidUtf8(replacementUtf8) || shortcut.empty()) {
			++skippedCount;
			continue;
		}

		const auto duplicate = std::find_if(entries.begin(), entries.end(), [&](const Entry& entry) {
			return entry.shortcut == shortcut;
		});
		if (duplicate != entries.end()) {
			++skippedCount;
			continue;
		}

		entries.push_back({ shortcut, replacement });
		++importedCount;
	}

	if (!input.eof()) {
		return { ImportStatus::FileError, importedCount, skippedCount };
	}
	if (importedCount == 0) {
		return { ImportStatus::NoValidEntries, 0, skippedCount };
	}
	if (!writeEntries(entries)) {
		return { ImportStatus::StorageError, importedCount, skippedCount };
	}
	return { ImportStatus::Success, importedCount, skippedCount };
}

MacroService::ExportResult MacroService::exportToFile(const std::wstring& path) {
		std::vector<Entry> entries;
		if (!tryReadEntries(entries)) return ExportResult::StorageError;
	std::vector<std::pair<std::string, std::string>> encodedEntries;
	encodedEntries.reserve(entries.size());
	for (const auto& entry : entries) {
		if (entry.shortcut.find_first_of(L"\r\n") != std::wstring::npos ||
			entry.replacement.find_first_of(L"\r\n") != std::wstring::npos) {
			return ExportResult::UnsupportedMultilineContent;
		}

		const std::string shortcut = wideToUtf8(entry.shortcut);
		const std::string replacement = wideToUtf8(entry.replacement);
		if (shortcut.empty() || (!entry.replacement.empty() && replacement.empty())) {
			return ExportResult::FileError;
		}
		encodedEntries.emplace_back(shortcut, replacement);
	}

	if (path.empty()) return ExportResult::FileError;
	std::error_code pathError;
	const auto destination = std::filesystem::absolute(std::filesystem::path(path), pathError);
	if (pathError || destination.filename().empty()) return ExportResult::FileError;
	const auto destinationName = destination.wstring();
	static volatile LONG sequence = 0;
	ExportTemporaryFile temporary;
	for (int attempt = 0; attempt < 8; ++attempt) {
		temporary.path = destinationName + L".openkey-export-" + std::to_wstring(GetCurrentProcessId()) +
			L"-" + std::to_wstring(GetTickCount64()) + L"-" +
			std::to_wstring(static_cast<DWORD>(InterlockedIncrement(&sequence))) + L".tmp";
		temporary.handle = CreateFileW(temporary.path.c_str(), GENERIC_WRITE, 0, nullptr,
			CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		if (temporary.handle != INVALID_HANDLE_VALUE) { temporary.owned = true; break; }
		const DWORD error = GetLastError();
		if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS) return ExportResult::FileError;
	}
	if (!temporary.owned) return ExportResult::FileError;
	if (!writeExport(temporary.handle, std::string(MacroFileHeader) + '\n')) return ExportResult::FileError;
	for (const auto& [shortcut, replacement] : encodedEntries) {
		if (!writeExport(temporary.handle, shortcut + ':' + replacement + '\n')) return ExportResult::FileError;
	}
	if (!FlushFileBuffers(temporary.handle) || !temporary.close()) return ExportResult::FileError;
	if (!MoveFileExW(temporary.path.c_str(), destinationName.c_str(),
		MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) return ExportResult::FileError;
	temporary.owned = false;
	return ExportResult::Success;
}
