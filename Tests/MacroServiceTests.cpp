#include <windows.h>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <type_traits>

namespace fixture {
std::vector<BYTE> blob;
bool present = false, openFails = false, createFails = false, writeFails = false;
bool missingValue = false, queryFails = false, readFails = false, readTypeChanges = false;
bool failReadAfterWrite = false;
bool exportCreateFails = false, exportWriteFails = false, exportFlushFails = false, exportMoveFails = false;
bool exportShortWrite = false, exportZeroWrite = false;
int exportCollisions = 0, exportCreateCalls = 0;
std::vector<std::wstring> exportOwnedPaths, exportCollisionPaths;
DWORD valueType = REG_BINARY;
int opened = 0, closed = 0, passed = 0;
int creates = 0, writes = 0;
std::filesystem::path directory;
void require(bool condition) { if (!condition) throw std::runtime_error("Assertion failed"); }
void validate(HKEY root, LPCWSTR path) {
    require(root == HKEY_CURRENT_USER && std::wstring(path) == L"SOFTWARE\\TuyenMai\\OpenKey");
}
LSTATUS open(HKEY root, LPCWSTR path, DWORD, REGSAM access, PHKEY key) {
    validate(root, path); require(access == KEY_READ);
    if (openFails) return ERROR_ACCESS_DENIED;
    if (!present) return ERROR_FILE_NOT_FOUND;
    *key = reinterpret_cast<HKEY>(1); ++opened; return ERROR_SUCCESS;
}
LSTATUS create(HKEY root, LPCWSTR path, DWORD, LPWSTR, DWORD, REGSAM access,
    LPSECURITY_ATTRIBUTES, PHKEY key, LPDWORD disposition) {
    validate(root, path); require(access == KEY_SET_VALUE);
    ++creates;
    if (createFails) return ERROR_ACCESS_DENIED;
    *key = reinterpret_cast<HKEY>(1); *disposition = REG_OPENED_EXISTING_KEY; ++opened; return ERROR_SUCCESS;
}
LSTATUS query(HKEY, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size) {
    require(std::wstring(name) == L"macroData");
    if (missingValue) return ERROR_FILE_NOT_FOUND;
    if (queryFails || (data && readFails)) return ERROR_ACCESS_DENIED;
    *type = valueType;
    if (data && readTypeChanges) *type = REG_SZ;
    const DWORD needed = static_cast<DWORD>(blob.size());
    if (data && *size < needed) { *size = needed; return ERROR_MORE_DATA; }
    if (data && needed) std::memcpy(data, blob.data(), needed);
    *size = needed; return ERROR_SUCCESS;
}
LSTATUS set(HKEY, LPCWSTR name, DWORD, DWORD type, const BYTE* data, DWORD size) {
    require(std::wstring(name) == L"macroData" && type == REG_BINARY);
    ++writes;
    if (writeFails) return ERROR_ACCESS_DENIED;
    blob.assign(data, data + size); valueType = type; present = true; missingValue = false;
    if (failReadAfterWrite) queryFails = true;
    return ERROR_SUCCESS;
}
LSTATUS close(HKEY key) { require(key == reinterpret_cast<HKEY>(1)); ++closed; return ERROR_SUCCESS; }
HANDLE exportCreate(LPCWSTR path, DWORD access, DWORD sharing, LPSECURITY_ATTRIBUTES security,
    DWORD disposition, DWORD attributes, HANDLE templateFile) {
    ++exportCreateCalls;
    require(disposition == CREATE_NEW && access == GENERIC_WRITE && sharing == 0);
    if (exportCreateFails) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
    if (exportCollisions > 0) {
        --exportCollisions;
        std::ofstream existing(std::filesystem::path(path), std::ios::binary);
        existing << "other-writer"; existing.close(); require(static_cast<bool>(existing));
        exportCollisionPaths.push_back(path);
        SetLastError(ERROR_FILE_EXISTS); return INVALID_HANDLE_VALUE;
    }
    auto handle = ::CreateFileW(path, access, sharing, security, disposition, attributes, templateFile);
    if (handle != INVALID_HANDLE_VALUE) exportOwnedPaths.push_back(path);
    return handle;
}
BOOL exportWrite(HANDLE handle, LPCVOID data, DWORD count, LPDWORD written, LPOVERLAPPED overlapped) {
    if (exportWriteFails) { *written = 0; SetLastError(ERROR_DISK_FULL); return FALSE; }
    if (exportZeroWrite) { *written = 0; return TRUE; }
    return ::WriteFile(handle, data, exportShortWrite && count > 1 ? count / 2 : count, written, overlapped);
}
BOOL exportFlush(HANDLE handle) {
    if (exportFlushFails) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
    return ::FlushFileBuffers(handle);
}
BOOL exportMove(LPCWSTR source, LPCWSTR target, DWORD flags) {
    require(flags == (MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH));
    require(std::filesystem::path(source).parent_path() == std::filesystem::path(target).parent_path());
    if (exportMoveFails) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return ::MoveFileExW(source, target, flags);
}
template<typename Action> void test(const char* name, Action action) {
    blob.clear(); present = openFails = createFails = writeFails = false;
    missingValue = queryFails = readFails = readTypeChanges = false;
    failReadAfterWrite = false;
    exportCreateFails = exportWriteFails = exportFlushFails = exportMoveFails = false;
    exportShortWrite = exportZeroWrite = false;
    exportCollisions = exportCreateCalls = 0;
    exportOwnedPaths.clear(); exportCollisionPaths.clear();
    valueType = REG_BINARY; opened = closed = 0;
    creates = writes = 0;
    try {
        action(); require(opened == closed);
        for (const auto& path : exportOwnedPaths) require(!std::filesystem::exists(path));
        for (const auto& path : exportCollisionPaths) {
            std::ifstream existing(std::filesystem::path(path), std::ios::binary);
            require(std::string(std::istreambuf_iterator<char>(existing), {}) == "other-writer");
        }
        ++passed;
    }
    catch (...) { std::cerr << "Failed: " << name << '\n'; throw; }
}
std::wstring file(const char* name) { return (directory / name).wstring(); }
void writeFile(const char* name, const std::string& content) {
    std::ofstream output(directory / name, std::ios::binary | std::ios::trunc);
    output.write(content.data(), static_cast<std::streamsize>(content.size())); require(static_cast<bool>(output));
}
std::string readFile(const char* name) {
    std::ifstream input(directory / name, std::ios::binary);
    require(static_cast<bool>(input)); return { std::istreambuf_iterator<char>(input), {} };
}
}
#define RegOpenKeyExW fixture::open
#define RegCreateKeyExW fixture::create
#define RegQueryValueExW fixture::query
#define RegSetValueExW fixture::set
#define RegCloseKey fixture::close
#define CreateFileW fixture::exportCreate
#define WriteFile fixture::exportWrite
#define FlushFileBuffers fixture::exportFlush
#define MoveFileExW fixture::exportMove
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/MacroService.cpp"
#undef CreateFileW
#undef WriteFile
#undef FlushFileBuffers
#undef MoveFileExW
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/MacroEditorState.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.UI/UiOperationGate.h"

int wmain(int argc, wchar_t** argv) {
    using namespace fixture;
    using S = MacroService;
    using R = S::SaveResult;
    using I = S::ImportStatus;
    using E = S::ExportResult;
    if (argc != 2) return 2;
    directory = argv[1];
    try {
        require(std::filesystem::is_directory(directory));
        test("empty store", [] { require(S::readEntries().empty()); });
        test("checked missing store is empty", [] {
            std::vector<S::Entry> entries{{L"keep", L"draft"}};
            require(S::tryReadEntries(entries) && entries.empty());
        });
        test("checked missing value is empty", [] {
            present = missingValue = true;
            std::vector<S::Entry> entries{{L"keep", L"draft"}};
            require(S::tryReadEntries(entries) && entries.empty());
        });
        test("checked valid Unicode", [] {
            require(S::saveEntry({L"tv", L"Tiếng Việt 😀 e\u0301"}, L"") == R::Success);
            std::vector<S::Entry> entries;
            require(S::tryReadEntries(entries) && entries.size() == 1);
            require(entries[0].shortcut == L"tv" && entries[0].replacement == L"Tiếng Việt 😀 e\u0301");
        });
        test("checked empty binary list", [] {
            present = true; blob = {0, 0};
            std::vector<S::Entry> entries{{L"keep", L"draft"}};
            require(S::tryReadEntries(entries) && entries.empty());
        });
        test("checked read failures preserve caller", [] {
            require(S::saveEntry({L"a", L"b"}, L"") == R::Success);
            for (auto flag : {&openFails, &queryFails, &readFails, &readTypeChanges}) {
                *flag = true;
                std::vector<S::Entry> entries{{L"keep", L"draft"}};
                require(!S::tryReadEntries(entries));
                require(entries.size() == 1 && entries[0].shortcut == L"keep" && entries[0].replacement == L"draft");
                *flag = false;
            }
        });
        test("checked wrong type preserves caller", [] {
            present = true; valueType = REG_SZ; blob = {0, 0};
            std::vector<S::Entry> entries{{L"keep", L"draft"}};
            require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].shortcut == L"keep");
        });
        test("checked truncated list preserves caller", [] {
            require(S::saveEntry({L"a", L"b"}, L"") == R::Success);
            require(S::saveEntry({L"c", L"d"}, L"") == R::Success);
            const auto valid = blob;
            for (size_t length = 0; length < valid.size(); ++length) {
                blob.assign(valid.begin(), valid.begin() + length);
                std::vector<S::Entry> entries{{L"keep", L"draft"}};
                require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].replacement == L"draft");
            }
        });
        test("checked malformed payload preserves caller", [] {
            present = true;
            const std::vector<std::vector<BYTE>> malformed{
                {0, 0, 1}, {1, 0, 0, 0, 0}, {255, 255},
                {1, 0, 1, 255, 1, 0, 'b'}, {1, 0, 1, 'a', 1, 0, 255},
                {1, 0, 1, 'a', 1, 0, 'b', 0}
            };
            for (const auto& payload : malformed) {
                blob = payload;
                std::vector<S::Entry> entries{{L"keep", L"draft"}};
                require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].replacement == L"draft");
            }
        });
        test("add Unicode", [] {
            require(S::saveEntry({ L"tv", L"Tiếng Việt 😀 e\u0301" }, L"") == R::Success);
            const auto entries = S::readEntries(); require(entries.size() == 1);
            require(entries[0].shortcut == L"tv" && entries[0].replacement == L"Tiếng Việt 😀 e\u0301");
        });
        test("read errors block mutations and export", [] {
            require(S::saveEntry({L"a", L"old"}, L"") == R::Success);
            writeFile("guard-import.txt", std::string(MacroFileHeader) + "\nb:new\n");
            writeFile("guard-export.txt", "keep-me");
            const auto old = blob; const int oldCreates = creates, oldWrites = writes;
            for (auto flag : {&openFails, &queryFails, &readFails, &readTypeChanges}) {
                *flag = true;
                require(S::saveEntry({L"b", L"new"}, L"") == R::StorageError);
                require(S::saveEntry({L"a", L"changed"}, L"a") == R::StorageError);
                require(!S::deleteEntry(L"a"));
                const auto result = S::importFromFile(file("guard-import.txt"), true);
                require(result.status == I::StorageError && result.importedCount == 0);
                require(S::exportToFile(file("guard-export.txt")) == E::StorageError);
                require(blob == old && creates == oldCreates && writes == oldWrites);
                require(readFile("guard-export.txt") == "keep-me");
                *flag = false;
            }
        });
        test("malformed store blocks mutations and export", [] {
            present = true;
            writeFile("malformed-import.txt", std::string(MacroFileHeader) + "\nb:new\n");
            writeFile("malformed-export.txt", "keep-me");
            for (const auto& payload : std::vector<std::vector<BYTE>>{
                {}, {1}, {255,255}, {1,0,1,'a',1,0,'b',0},
                {2,0,1,'a',1,0,'b',1,'c',2,0,'d'}, {1,0,1,255,1,0,'b'}}) {
                blob = payload;
                require(S::saveEntry({L"b", L"new"}, L"") == R::StorageError);
                require(!S::deleteEntry(L"a"));
                require(S::importFromFile(file("malformed-import.txt"), true).status == I::StorageError);
                require(S::exportToFile(file("malformed-export.txt")) == E::StorageError);
                require(blob == payload && creates == 0 && writes == 0);
                require(readFile("malformed-export.txt") == "keep-me");
            }
            valueType = REG_SZ; blob = {0,0};
            require(S::saveEntry({L"b",L"new"},L"") == R::StorageError);
            require(!S::deleteEntry(L"a"));
            require(S::importFromFile(file("malformed-import.txt"), true).status == I::StorageError);
            require(S::exportToFile(file("malformed-export.txt")) == E::StorageError);
            require(creates == 0 && writes == 0 && readFile("malformed-export.txt") == "keep-me");
        });
        test("merge into missing store", [] {
            writeFile("missing-store-import.txt", std::string(MacroFileHeader) + "\na:new\n");
            require(S::importFromFile(file("missing-store-import.txt"), true).status == I::Success);
            require(S::readEntries().size() == 1);
        });
        test("save into missing value", [] {
            present = missingValue = true;
            require(S::saveEntry({L"a",L"new"},L"") == R::Success);
            require(S::readEntries().size() == 1);
        });
        test("explicit replace recovers malformed store", [] {
            present = true; blob = {255,255};
            writeFile("recover.txt", std::string(MacroFileHeader) + "\na:new\n");
            require(S::importFromFile(file("recover.txt"), false).status == I::Success);
            std::vector<S::Entry> entries;
            require(S::tryReadEntries(entries) && entries.size() == 1 && entries[0].replacement == L"new");
        });
        test("empty replacement legacy entry preserved", [] {
            present = true; blob = {1,0,1,'a',0,0};
            require(S::saveEntry({L"b",L"new"},L"") == R::Success);
            std::vector<S::Entry> entries;
            require(S::tryReadEntries(entries) && entries.size() == 2 && entries[0].replacement.empty());
        });
        test("missing store export is valid empty backup", [] {
            require(S::exportToFile(file("empty-export.txt")) == E::Success);
            require(readFile("empty-export.txt") == std::string(MacroFileHeader) + "\n");
        });
        test("failed read does not create export file", [] {
            openFails = true;
            require(S::exportToFile(file("not-created.txt")) == E::StorageError);
            require(!std::filesystem::exists(directory / "not-created.txt"));
        });
        test("legacy binary bytes", [] {
            require(S::saveEntry({ L"a", L"bc" }, L"") == R::Success);
            require(blob == std::vector<BYTE>({ 1, 0, 1, 'a', 2, 0, 'b', 'c' }));
        });
        test("save completed readback failure preserves snapshot", [] {
            std::vector<S::Entry> entries{{L"keep",L"draft"}};
            failReadAfterWrite = true;
            require(S::saveEntry({L"a",L"new"},L"") == R::Success && writes == 1);
            const auto saved = blob;
            require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].replacement == L"draft");
            require(blob == saved && writes == 1);
            queryFails = false;
            require(S::tryReadEntries(entries) && entries.size() == 1 && entries[0].replacement == L"new");
        });
        test("delete completed readback failure preserves snapshot", [] {
            require(S::saveEntry({L"a",L"old"},L"") == R::Success);
            std::vector<S::Entry> entries;
            require(S::tryReadEntries(entries)); failReadAfterWrite = true;
            require(S::deleteEntry(L"a") && writes == 2);
            require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].shortcut == L"a");
            queryFails = false;
            require(S::tryReadEntries(entries) && entries.empty() && writes == 2);
        });
        test("import completed readback failure preserves snapshot", [] {
            require(S::saveEntry({L"a",L"old"},L"") == R::Success);
            std::vector<S::Entry> entries;
            require(S::tryReadEntries(entries)); failReadAfterWrite = true;
            writeFile("readback-import.txt",std::string(MacroFileHeader) + "\nb:new\n");
            require(S::importFromFile(file("readback-import.txt"),true).status == I::Success && writes == 2);
            require(!S::tryReadEntries(entries) && entries.size() == 1 && entries[0].shortcut == L"a");
            queryFails = false;
            require(S::tryReadEntries(entries) && entries.size() == 2 && writes == 2);
        });
        test("duplicate preserves storage", [] {
            require(S::saveEntry({ L"a", L"one" }, L"") == R::Success); const auto old = blob;
            require(S::saveEntry({ L"a", L"two" }, L"") == R::DuplicateShortcut && blob == old);
        });
        test("edit rename", [] {
            require(S::saveEntry({ L"a", L"one" }, L"") == R::Success);
            require(S::saveEntry({ L"b", L"two" }, L"a") == R::Success);
            const auto entries = S::readEntries(); require(entries.size() == 1 && entries[0].shortcut == L"b" && entries[0].replacement == L"two");
        });
        test("rename conflict", [] {
            require(S::saveEntry({ L"a", L"one" }, L"") == R::Success);
            require(S::saveEntry({ L"b", L"two" }, L"") == R::Success); const auto old = blob;
            require(S::saveEntry({ L"b", L"changed" }, L"a") == R::DuplicateShortcut && blob == old);
        });
        test("delete", [] {
            require(S::saveEntry({ L"a", L"one" }, L"") == R::Success);
            require(S::deleteEntry(L"a") && S::readEntries().empty());
            const auto old = blob; require(!S::deleteEntry(L"missing") && blob == old);
        });
        test("empty fields", [] {
            require(S::saveEntry({ L"", L"x" }, L"") == R::EmptyShortcut);
            require(S::saveEntry({ L"a", L"" }, L"") == R::EmptyReplacement && !present);
        });
        test("shortcut byte boundary", [] {
            require(S::saveEntry({ std::wstring(85, L'ệ'), L"x" }, L"") == R::Success); const auto old = blob;
            require(S::saveEntry({ std::wstring(86, L'ệ'), L"x" }, L"") == R::ShortcutTooLong && blob == old);
        });
        test("replacement byte boundary", [] {
            require(S::saveEntry({ L"a", std::wstring(65535, L'x') }, L"") == R::Success); const auto old = blob;
            require(S::saveEntry({ L"a", std::wstring(65536, L'x') }, L"a") == R::ReplacementTooLong && blob == old);
        });
        test("invalid UTF16", [] {
            require(S::saveEntry({ std::wstring(1, wchar_t(0xD800)), L"x" }, L"") == R::ShortcutTooLong);
            require(S::saveEntry({ L"a", std::wstring(1, wchar_t(0xD800)) }, L"") == R::ReplacementTooLong && !present);
        });
        test("create denied", [] { createFails = true; require(S::saveEntry({ L"a", L"x" }, L"") == R::StorageError && !present); });
        test("write denied", [] {
            require(S::saveEntry({ L"a", L"old" }, L"") == R::Success); const auto old = blob; writeFails = true;
            require(S::saveEntry({ L"a", L"new" }, L"a") == R::StorageError && blob == old);
            require(!S::deleteEntry(L"a") && blob == old);
        });
        test("wrong registry type", [] { present = true; valueType = REG_SZ; blob = { 1, 0 }; require(S::readEntries().empty()); });
        test("truncated binary", [] {
            require(S::saveEntry({ L"a", L"bc" }, L"") == R::Success); const auto valid = blob;
            for (size_t length = 0; length < valid.size(); ++length) {
                blob.assign(valid.begin(), valid.begin() + length); require(S::readEntries().empty());
            }
        });
        test("import BOM CRLF Unicode", [] {
            writeFile("import.txt", std::string("\xEF\xBB\xBF") + MacroFileHeader + "\r\n:):vui 😀\r\ntv:Tiếng Việt\r\n");
            const auto result = S::importFromFile(file("import.txt"), false);
            require(result.status == I::Success && result.importedCount == 2 && result.skippedCount == 0);
            const auto entries = S::readEntries(); require(entries.size() == 2 && entries[0].shortcut == L":)" && entries[1].replacement == L"Tiếng Việt");
        });
        test("merge skips duplicate malformed UTF8", [] {
            require(S::saveEntry({ L"a", L"old" }, L"") == R::Success);
            writeFile("merge.txt", std::string(MacroFileHeader) + "\na:new\nb:valid\nb:duplicate\nbad\nc:\xFF\n");
            const auto result = S::importFromFile(file("merge.txt"), true);
            require(result.status == I::Success && result.importedCount == 1 && result.skippedCount == 4);
            const auto entries = S::readEntries(); require(entries.size() == 2 && entries[0].replacement == L"old");
        });
        test("replace import", [] {
            require(S::saveEntry({ L"old", L"old" }, L"") == R::Success);
            writeFile("replace.txt", std::string(MacroFileHeader) + "\na:new\n");
            require(S::importFromFile(file("replace.txt"), false).status == I::Success);
            const auto entries = S::readEntries(); require(entries.size() == 1 && entries[0].shortcut == L"a");
        });
        test("invalid import preserves blob", [] {
            require(S::saveEntry({ L"a", L"old" }, L"") == R::Success); const auto old = blob;
            writeFile("invalid.txt", "wrong header\na:new\n");
            require(S::importFromFile(file("invalid.txt"), false).status == I::InvalidFormat && blob == old);
            writeFile("invalid.txt", std::string(MacroFileHeader) + "\nmalformed\n");
            require(S::importFromFile(file("invalid.txt"), false).status == I::NoValidEntries && blob == old);
        });
        test("missing import", [] { require(S::importFromFile(file("missing.txt"), true).status == I::FileError && !present); });
        test("import storage failure", [] {
            writeFile("denied.txt", std::string(MacroFileHeader) + "\na:new\n"); writeFails = true;
            const auto result = S::importFromFile(file("denied.txt"), false);
            require(result.status == I::StorageError && result.importedCount == 1 && !present);
        });
        test("export import roundtrip", [] {
            require(S::saveEntry({ L":)", L"Tiếng Việt 😀 e\u0301:colon" }, L"") == R::Success); const auto old = blob;
            require(S::exportToFile(file("roundtrip.txt")) == E::Success);
            blob.clear(); present = false;
            require(S::importFromFile(file("roundtrip.txt"), false).status == I::Success && blob == old);
        });
        test("multiline export preserves destination", [] {
            require(S::saveEntry({ L"a", L"first\nsecond" }, L"") == R::Success);
            writeFile("sentinel.txt", "keep-me");
            require(S::exportToFile(file("sentinel.txt")) == E::UnsupportedMultilineContent);
            require(readFile("sentinel.txt") == "keep-me");
        });
        test("export path failure", [] { require(S::exportToFile((directory / "absent" / "export.txt").wstring()) == E::FileError); });
        test("export empty path rejected", [] { require(S::exportToFile(L"") == E::FileError && exportCreateCalls == 0); });
        test("export create failure preserves old backup", [] {
            writeFile("create-failure.txt", "keep-me"); exportCreateFails = true;
            require(S::exportToFile(file("create-failure.txt")) == E::FileError);
            require(readFile("create-failure.txt") == "keep-me");
        });
        test("export write failure preserves old backup", [] {
            writeFile("write-failure.txt", "keep-me"); exportWriteFails = true;
            require(S::exportToFile(file("write-failure.txt")) == E::FileError);
            require(readFile("write-failure.txt") == "keep-me");
        });
        test("export zero progress fails safely", [] {
            writeFile("zero-write.txt", "keep-me"); exportZeroWrite = true;
            require(S::exportToFile(file("zero-write.txt")) == E::FileError);
            require(readFile("zero-write.txt") == "keep-me");
        });
        test("export partial writes finish", [] {
            require(S::saveEntry({ L"dc", L"được 😀" }, L"") == R::Success);
            exportShortWrite = true;
            require(S::exportToFile(file("partial-write.txt")) == E::Success);
            require(readFile("partial-write.txt") == std::string(MacroFileHeader) + "\ndc:" + wideToUtf8(L"được 😀") + "\n");
        });
        test("export flush failure preserves old backup", [] {
            writeFile("flush-failure.txt", "keep-me"); exportFlushFails = true;
            require(S::exportToFile(file("flush-failure.txt")) == E::FileError);
            require(readFile("flush-failure.txt") == "keep-me");
        });
        test("export move failure preserves old backup", [] {
            writeFile("move-failure.txt", "keep-me"); exportMoveFails = true;
            require(S::exportToFile(file("move-failure.txt")) == E::FileError);
            require(readFile("move-failure.txt") == "keep-me");
        });
        test("export collision retries without deleting other file", [] {
            exportCollisions = 1;
            require(S::exportToFile(file("collision.txt")) == E::Success && exportCreateCalls == 2);
        });
        test("export collision exhaustion preserves backup", [] {
            writeFile("collisions.txt", "keep-me"); exportCollisions = 8;
            require(S::exportToFile(file("collisions.txt")) == E::FileError && exportCreateCalls == 8);
            require(readFile("collisions.txt") == "keep-me");
        });
        test("export replaces old backup completely", [] {
            writeFile("replace.txt", std::string(4096, 'x'));
            require(S::exportToFile(file("replace.txt")) == E::Success);
            require(readFile("replace.txt") == std::string(MacroFileHeader) + "\n");
        });
        test("export locked destination preserves backup", [] {
            writeFile("locked.txt", "keep-me");
            const auto locked = ::CreateFileW(file("locked.txt").c_str(), GENERIC_READ, FILE_SHARE_READ,
                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            require(locked != INVALID_HANDLE_VALUE);
            const auto result = S::exportToFile(file("locked.txt"));
            require(::CloseHandle(locked) != FALSE);
            require(result == E::FileError && readFile("locked.txt") == "keep-me");
        });
        test("export directory target is not removed", [] {
            const auto target = directory / "destination-directory";
            require(std::filesystem::create_directory(target));
            require(S::exportToFile(target.wstring()) == E::FileError && std::filesystem::is_directory(target));
        });
        test("empty editor is clean", [] {
            require(!OpenKeyUIMacroEditor::HasUnsavedChanges({}, L"", L"", L""));
        });
        test("new shortcut draft", [] {
            require(OpenKeyUIMacroEditor::HasUnsavedChanges({}, L"", L"dc", L""));
        });
        test("new replacement draft", [] {
            require(OpenKeyUIMacroEditor::HasUnsavedChanges({}, L"", L"", L"được"));
        });
        test("selected unchanged entry is clean", [] {
            require(!OpenKeyUIMacroEditor::HasUnsavedChanges({{ L"dc", L"được" }}, L"dc", L"dc", L"được"));
        });
        test("renamed shortcut draft", [] {
            require(OpenKeyUIMacroEditor::HasUnsavedChanges({{ L"dc", L"được" }}, L"dc", L"duoc", L"được"));
        });
        test("edited replacement draft", [] {
            require(OpenKeyUIMacroEditor::HasUnsavedChanges({{ L"dc", L"được" }}, L"dc", L"dc", L"đã được"));
        });
        test("missing original preserves draft", [] {
            require(OpenKeyUIMacroEditor::HasUnsavedChanges({}, L"dc", L"dc", L"được"));
        });
        test("reverted edits are clean", [] {
            require(!OpenKeyUIMacroEditor::HasUnsavedChanges({{ L"a", L"first" }, { L"dc", L"được 😀" }}, L"dc", L"dc", L"được 😀"));
        });
        test("gate starts idle", [] { OpenKeyUI::OperationGate gate; require(!gate.Busy()); });
        test("gate releases at scope end", [] {
            OpenKeyUI::OperationGate gate;
            { auto lease = gate.TryBegin(); require(static_cast<bool>(lease) && gate.Busy()); }
            require(!gate.Busy());
        });
        test("gate rejects overlapping operation", [] {
            OpenKeyUI::OperationGate gate; auto lease = gate.TryBegin();
            auto rejected = gate.TryBegin(); require(!rejected && gate.Busy());
        });
        test("rejected scope cannot release owner", [] {
            OpenKeyUI::OperationGate gate; auto lease = gate.TryBegin();
            { auto rejected = gate.TryBegin(); require(!rejected); }
            require(gate.Busy());
        });
        test("exception releases gate", [] {
            OpenKeyUI::OperationGate gate;
            try { auto lease = gate.TryBegin(); require(static_cast<bool>(lease)); throw std::runtime_error("fixture"); }
            catch (std::runtime_error const&) {}
            require(!gate.Busy()); auto retry = gate.TryBegin(); require(static_cast<bool>(retry));
        });
        test("early release idempotent", [] {
            OpenKeyUI::OperationGate gate; auto lease = gate.TryBegin();
            lease.Release(); lease.Release(); require(!lease && !gate.Busy());
            auto next = gate.TryBegin(); require(static_cast<bool>(next) && gate.Busy());
            lease.Release(); require(gate.Busy());
        });
        test("old released lease cannot release next scope", [] {
            OpenKeyUI::OperationGate gate;
            { auto old = gate.TryBegin(); old.Release(); auto current = gate.TryBegin();
              { auto blocked = gate.TryBegin(); require(!blocked); } require(gate.Busy()); }
            require(!gate.Busy());
        });
        test("repeated cancellation releases gate", [] {
            static_assert(!std::is_copy_constructible_v<OpenKeyUI::OperationGate::Lease>);
            static_assert(!std::is_move_constructible_v<OpenKeyUI::OperationGate::Lease>);
            OpenKeyUI::OperationGate gate;
            for (int i = 0; i < 100; ++i) { auto lease = gate.TryBegin(); require(static_cast<bool>(lease)); }
            require(!gate.Busy());
        });
        std::cout << "Macro service/editor/gate: " << passed << "/" << passed << " passed (fake registry, pure state; isolated files)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
