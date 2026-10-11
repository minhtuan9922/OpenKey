#pragma once

#include <cstddef>
#include <string>
#include <vector>

class MacroService {
public:
	struct Entry {
		std::wstring shortcut;
		std::wstring replacement;
	};
	enum class SaveResult {
		Success,
		EmptyShortcut,
		EmptyReplacement,
		ShortcutTooLong,
		ReplacementTooLong,
		DuplicateShortcut,
		StorageError
	};
	enum class ImportStatus {
		Success,
		FileError,
		InvalidFormat,
		NoValidEntries,
		StorageError
	};
	struct ImportResult {
		ImportStatus status;
		std::size_t importedCount;
		std::size_t skippedCount;
	};
	enum class ExportResult {
		Success,
		FileError,
		UnsupportedMultilineContent,
		StorageError
	};

	static std::vector<Entry> readEntries();
	// Missing data is empty. Errors/malformed data leave the list untouched.
	static bool tryReadEntries(std::vector<Entry>& entries);
	static SaveResult saveEntry(const Entry& entry, const std::wstring& originalShortcut);
	static bool deleteEntry(const std::wstring& shortcut);
	static ImportResult importFromFile(const std::wstring& path, bool keepExisting);
	static ExportResult exportToFile(const std::wstring& path);
};
