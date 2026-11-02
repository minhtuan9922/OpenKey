#define NOMINMAX
#include "CaretDeployment.h"
#include <shlobj.h>
#include <shellapi.h>
#include <bcrypt.h>
#include <sddl.h>
#include <aclapi.h>
#include <cstring>

namespace {
    struct Resource { const BYTE* bytes = nullptr; DWORD size = 0; };
    Resource Payload(unsigned id) {
        HMODULE module = GetModuleHandleW(nullptr);
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) return {};
        return {static_cast<const BYTE*>(LockResource(LoadResource(module, resource))), SizeofResource(module, resource)};
    }
    std::wstring Digest(Resource resource) {
        BCRYPT_ALG_HANDLE algorithm = nullptr; BYTE digest[32]{};
        if (!resource.bytes || !resource.size || BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) < 0) return {};
        const auto status = BCryptHash(algorithm, nullptr, 0, const_cast<BYTE*>(resource.bytes), resource.size, digest, sizeof(digest));
        BCryptCloseAlgorithmProvider(algorithm, 0);
        if (status < 0) return {};
        std::wstring result;
        for (BYTE value : digest) { result += L"0123456789abcdef"[value >> 4]; result += L"0123456789abcdef"[value & 15]; }
        return result;
    }
    bool Architecture(Resource resource, WORD machine) {
        if (!resource.bytes || resource.size < sizeof(IMAGE_DOS_HEADER)) return false;
        IMAGE_DOS_HEADER dos{}; std::memcpy(&dos, resource.bytes, sizeof(dos));
        if (dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < static_cast<LONG>(sizeof(dos)) ||
            static_cast<ULONGLONG>(dos.e_lfanew) + sizeof(DWORD) + sizeof(WORD) > resource.size) return false;
        DWORD signature = 0; WORD actual = 0;
        std::memcpy(&signature, resource.bytes + dos.e_lfanew, sizeof(signature));
        std::memcpy(&actual, resource.bytes + dos.e_lfanew + sizeof(signature), sizeof(actual));
        return signature == IMAGE_NT_SIGNATURE && actual == machine;
    }
    bool Administrator() {
        SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY; PSID group = nullptr; BOOL member = FALSE;
        if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &group)) {
            CheckTokenMembership(nullptr, group, &member); FreeSid(group);
        }
        return member != FALSE;
    }
    struct Security {
        PSECURITY_DESCRIPTOR value = nullptr;
        Security() { ConvertStringSecurityDescriptorToSecurityDescriptorW(L"O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;GRGX;;;BU)", SDDL_REVISION_1, &value, nullptr); }
        ~Security() { if (value) LocalFree(value); }
    };
    bool Protected(const std::wstring& path) {
        PSECURITY_DESCRIPTOR security = nullptr; PACL acl = nullptr; PSID owner = nullptr;
        if (GetNamedSecurityInfoW(path.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, &owner, nullptr, &acl, nullptr, &security) != ERROR_SUCCESS) return false;
        BYTE system[SECURITY_MAX_SID_SIZE]{}, administrators[SECURITY_MAX_SID_SIZE]{};
        DWORD systemSize = sizeof(system), adminSize = sizeof(administrators);
        bool safe = acl && CreateWellKnownSid(WinLocalSystemSid, nullptr, system, &systemSize) &&
            CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, administrators, &adminSize);
        safe = safe && owner && (EqualSid(owner, system) || EqualSid(owner, administrators));
        for (DWORD i = 0; safe && i < acl->AceCount; ++i) {
            void* value = nullptr; if (!GetAce(acl, i, &value)) { safe = false; break; }
            const auto* header = static_cast<ACE_HEADER*>(value);
            if (header->AceType == ACCESS_DENIED_ACE_TYPE) continue;
            if (header->AceType != ACCESS_ALLOWED_ACE_TYPE) { safe = false; break; }
            const auto* ace = static_cast<ACCESS_ALLOWED_ACE*>(value);
            if (ace->Mask & (FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_DELETE_CHILD | DELETE | WRITE_DAC | WRITE_OWNER | GENERIC_WRITE | GENERIC_ALL))
                safe = EqualSid(const_cast<DWORD*>(&ace->SidStart), system) || EqualSid(const_cast<DWORD*>(&ace->SidStart), administrators);
        }
        LocalFree(security); return safe;
    }
    bool Directory(const std::wstring& path, bool create) {
        if (create) {
            Security security; if (!security.value) return false;
            SECURITY_ATTRIBUTES attributes{sizeof(attributes), security.value, FALSE};
            if (!CreateDirectoryW(path.c_str(), &attributes) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
        }
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) &&
            !(attributes & FILE_ATTRIBUTE_REPARSE_POINT) && Protected(path);
    }
    bool Matches(const std::wstring& path, Resource payload) {
        HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        BY_HANDLE_FILE_INFORMATION info{}; LARGE_INTEGER size{};
        bool valid = GetFileInformationByHandle(file, &info) && !(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
            GetFileSizeEx(file, &size) && size.QuadPart == payload.size;
        std::vector<BYTE> bytes(payload.size); DWORD read = 0;
        valid = valid && ReadFile(file, bytes.data(), payload.size, &read, nullptr) && read == payload.size &&
            std::memcmp(bytes.data(), payload.bytes, payload.size) == 0;
        CloseHandle(file); return valid && Protected(path);
    }
    bool WritePayload(const std::wstring& path, Resource payload) {
        if (Matches(path, payload)) return true;
        // Immutable version directory: never overwrite a loaded or unexpected DLL.
        Security security; if (!security.value) return false;
        SECURITY_ATTRIBUTES attributes{sizeof(attributes), security.value, FALSE};
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, &attributes, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0; const bool valid = WriteFile(file, payload.bytes, payload.size, &written, nullptr) && written == payload.size && FlushFileBuffers(file);
        CloseHandle(file); if (!valid) DeleteFileW(path.c_str()); return valid;
    }
    DWORD Register(const std::wstring& dll, bool x64, bool remove) {
        wchar_t windows[MAX_PATH]{}; if (!GetWindowsDirectoryW(windows, MAX_PATH)) return GetLastError();
        const bool os64 = OpenKeyCaretDeployment::Is64BitWindows();
        std::wstring folder = !x64 && os64 ? L"SysWOW64" : L"System32";
#ifndef _WIN64
        if (x64 && os64) folder = L"Sysnative";
#endif
        const std::wstring executable = std::wstring(windows) + L"\\" + folder + L"\\regsvr32.exe";
        std::wstring command = L"\"" + executable + L"\" /s " + (remove ? std::wstring(L"/u ") : std::wstring()) + L"\"" + dll + L"\"";
        STARTUPINFOW startup{}; startup.cb = sizeof(startup); PROCESS_INFORMATION process{};
        if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return GetLastError();
        CloseHandle(process.hThread); DWORD result = ERROR_TIMEOUT;
        if (WaitForSingleObject(process.hProcess, 60000) == WAIT_OBJECT_0) GetExitCodeProcess(process.hProcess, &result);
        CloseHandle(process.hProcess); return result;
    }
    DWORD Execute(const std::wstring& operation) {
        if ((operation == L"--install" || operation == L"--uninstall") && !Administrator()) return ERROR_ACCESS_DENIED;
        const bool os64 = OpenKeyCaretDeployment::Is64BitWindows();
        const Resource x64 = Payload(101), x86 = Payload(102);
        if (!Architecture(x64, IMAGE_FILE_MACHINE_AMD64) || !Architecture(x86, IMAGE_FILE_MACHINE_I386)) return ERROR_BAD_FORMAT;
        if (operation == L"--verify-payload") return ERROR_SUCCESS;
        const auto hash64 = Digest(x64), hash86 = Digest(x86);
        if (hash64.empty() || hash86.empty()) return ERROR_BAD_FORMAT;
        PWSTR folder = nullptr;
        std::wstring programFiles;
        if (SUCCEEDED(SHGetKnownFolderPath(os64 ? FOLDERID_ProgramFilesX64 : FOLDERID_ProgramFiles, 0, nullptr, &folder))) {
            programFiles = folder; CoTaskMemFree(folder);
        } else if (os64) {
            // FOLDERID_ProgramFilesX64 is not exposed to all WOW64 callers.
            // Read the native system path, never an inherited environment value.
            wchar_t native[MAX_PATH]{}; DWORD size = sizeof(native);
            if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion", L"ProgramFilesDir",
                RRF_RT_REG_SZ | RRF_SUBKEY_WOW6464KEY, nullptr, native, &size) != ERROR_SUCCESS) return ERROR_PATH_NOT_FOUND;
            programFiles = native;
        } else return ERROR_PATH_NOT_FOUND;
        const std::wstring root = programFiles + L"\\OpenKey-CaretEditing";
        const std::wstring version = root + L"\\" + hash64.substr(0, 32) + hash86.substr(0, 32);
        const std::wstring dll64 = version + L"\\x64\\OpenKey.TextService.dll", dll86 = version + L"\\x86\\OpenKey.TextService.dll";
        if (operation == L"--owns") {
            const auto registered64 = os64 ? OpenKeyCaretDeployment::RegisteredPath(true) : std::wstring();
            const auto registered86 = OpenKeyCaretDeployment::RegisteredPath(false);
            if ((!registered64.empty() && !OpenKeyCaretDeployment::SamePath(registered64, dll64)) ||
                (!registered86.empty() && !OpenKeyCaretDeployment::SamePath(registered86, dll86))) return ERROR_REVISION_MISMATCH;
            return !registered64.empty() || !registered86.empty() ? ERROR_SUCCESS : ERROR_NOT_FOUND;
        }
        if (operation == L"--status") {
            return Directory(root, false) && Directory(version, false) &&
                Directory(version + L"\\x86", false) && (!os64 || Directory(version + L"\\x64", false)) &&
                (!os64 || (OpenKeyCaretDeployment::SamePath(OpenKeyCaretDeployment::RegisteredPath(true), dll64) && Matches(dll64, x64))) &&
                OpenKeyCaretDeployment::SamePath(OpenKeyCaretDeployment::RegisteredPath(false), dll86) && Matches(dll86, x86) ? ERROR_SUCCESS : ERROR_NOT_FOUND;
        }
        if (!Administrator()) return ERROR_ACCESS_DENIED;
        if (operation == L"--install") {
            if (!Directory(root, true) || !Directory(version, true) || !Directory(version + L"\\x86", true) ||
                (os64 && !Directory(version + L"\\x64", true))) return ERROR_ACCESS_DENIED;
            if (!WritePayload(dll86, x86) || (os64 && !WritePayload(dll64, x64))) return ERROR_WRITE_FAULT;
            DWORD result = os64 ? Register(dll64, true, false) : ERROR_SUCCESS;
            if (!result) result = Register(dll86, false, false);
            return result;
        }
        if (operation == L"--uninstall") {
            const auto registered64 = os64 ? OpenKeyCaretDeployment::RegisteredPath(true) : std::wstring();
            const auto registered86 = OpenKeyCaretDeployment::RegisteredPath(false);
            // An older app must not unregister a newer/different installation.
            if ((!registered64.empty() && !OpenKeyCaretDeployment::SamePath(registered64, dll64)) ||
                (!registered86.empty() && !OpenKeyCaretDeployment::SamePath(registered86, dll86))) return ERROR_REVISION_MISMATCH;
            if (registered64.empty() && registered86.empty()) return ERROR_SUCCESS;
            if (!Directory(root, false) || !Directory(version, false) ||
                (!registered64.empty() && (!Directory(version + L"\\x64", false) || !Matches(dll64, x64))) ||
                (!registered86.empty() && (!Directory(version + L"\\x86", false) || !Matches(dll86, x86)))) return ERROR_INVALID_DATA;
            DWORD result = ERROR_SUCCESS;
            if (!registered64.empty()) result = Register(dll64, true, true);
            if (!result && !registered86.empty()) result = Register(dll86, false, true);
            // Keep versioned protected files: running editors may still hold them.
            return result;
        }
        return ERROR_INVALID_PARAMETER;
    }
}
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int count = 0; auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    DWORD result = ERROR_INVALID_PARAMETER;
    if (arguments && count == 2) {
        const std::wstring operation = arguments[1];
        if (operation == L"--install" || operation == L"--uninstall" || operation == L"--status" || operation == L"--owns" || operation == L"--verify-payload") {
            try { result = Execute(operation); } catch (...) { result = ERROR_OUTOFMEMORY; }
        }
    }
    if (arguments) LocalFree(arguments);
    if (SUCCEEDED(initialized)) CoUninitialize();
    return static_cast<int>(result);
}
