#include "../Sources/OpenKey/win32/OpenKey/OpenKey.TextService/CaretAutomation.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/CaretSettingsProtocol.h"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey.TextService/CaretTrace.h"
#include <iostream>
#include <thread>
#include <iomanip>
#include <cstdlib>
#include <cstring>

int CaptureTrace(unsigned seconds) {
    using namespace OpenKeyCaretTrace;
    HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Buffer), Name);
    if (!mapping) { std::cerr << "Cannot create diagnostic channel.\n"; return 1; }
    if (GetLastError() == ERROR_ALREADY_EXISTS) { CloseHandle(mapping); std::cerr << "A trace collector is already running.\n"; return 1; }
    auto* buffer = static_cast<Buffer*>(MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(Buffer)));
    if (!buffer) { CloseHandle(mapping); return 1; }
    ZeroMemory(buffer, sizeof(Buffer)); buffer->magic = Magic; InterlockedExchange(&buffer->enabled, 1);
    struct Cleanup { Buffer* buffer; HANDLE mapping; ~Cleanup() { InterlockedExchange(&buffer->enabled, 0); UnmapViewOfFile(buffer); CloseHandle(mapping); } } cleanup{buffer, mapping};
    std::cout << "Trace enabled for " << seconds << " seconds. Switch to VSCode; try nam + a at line end, then nam. + a before the period. Metadata only.\n" << std::flush;
    const auto until = GetTickCount64() + seconds * 1000ull;
    LONG last = 0; unsigned events = 0;
    while (GetTickCount64() < until) {
        const LONG newest = InterlockedCompareExchange(&buffer->next, 0, 0);
        if (newest - last > static_cast<LONG>(Capacity)) last = newest - Capacity;
        while (last < newest) {
            const LONG serial = last + 1;
            auto& slot = buffer->slots[static_cast<unsigned>(serial) % Capacity];
            if (InterlockedCompareExchange(&slot.committed, 0, 0) != serial) break;
            Record record; MemoryBarrier(); std::memcpy(&record, &slot.record, sizeof(record)); MemoryBarrier();
            if (InterlockedCompareExchange(&slot.committed, 0, 0) != serial) break;
            last = serial; ++events;
            std::cout << "{\"event\":" << events << ",\"build\":" << record.build
                << ",\"kind\":\"" << (record.kind == 2 ? "session" : record.kind ? "key" : "test") << "\",\"stage\":\"" << Label(record.stage)
                << "\",\"hresult\":\"0x" << std::hex << std::setw(8) << std::setfill('0') << static_cast<std::uint32_t>(record.hr) << std::dec
                << "\",\"before\":" << record.before << ",\"after\":" << record.after << ",\"word\":" << record.word
                << ",\"mode\":" << record.mode << ",\"flags\":" << record.flags << ",\"eaten\":" << record.eaten
                << ",\"beforeClass\":" << record.beforeClass << ",\"afterClass\":" << record.afterClass << "}\n" << std::flush;
        }
        Sleep(50);
    }
    std::cout << "{\"traceEvents\":" << events << "}\n";
    return 0;
}

// Optional interactive diagnostic. It never selects/replaces text, never logs
// the document, window title, word or surrounding context, and never sets focus.
int main(int argc, char** argv) {
    if (argc >= 2 && std::strcmp(argv[1], "--trace") == 0) {
        const unsigned seconds = argc >= 3 ? static_cast<unsigned>(std::atoi(argv[2])) : 45;
        if (seconds < 5 || seconds > 120) return 1;
        return CaptureTrace(seconds);
    }
    std::cout << "Within 3 seconds, focus the editor and put the caret in a Vietnamese word. Read-only diagnostic.\n";
    std::thread worker([] {
        Sleep(3000);
        const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        if (FAILED(initialized)) { std::cout << "{\"ready\":false,\"stage\":\"COM\"}\n"; return; }
        {
            OpenKeyTextService::AutomationReader reader;
            OpenKeyTextService::AutomationContext context;
            const bool ready = reader.Read(context, [] { return true; });
            const HWND foreground = GetForegroundWindow();
            bool enabled = false, modern = false, intent = false, active = false;
            int inputType = 0; DWORD_PTR packet = 0;
            const HWND engine = FindWindowW(L"OpenKeyVietnameseInputMethod", nullptr);
            const bool protocol = engine && SendMessageTimeoutW(engine, OpenKeyCaretRuntime::Message(), reinterpret_cast<WPARAM>(foreground), 0,
                SMTO_ABORTIFHUNG | SMTO_BLOCK, 50, &packet) &&
                OpenKeyCaretRuntime::Decode(packet, enabled, inputType, modern, &intent, &active);
            std::cout << "{\"uiaReadable\":" << (ready ? "true" : "false") << ",\"stage\":\"" << reader.Stage()
                << "\",\"wordLength\":" << context.word.text.size()
                << ",\"runtimeProtocol\":" << (protocol ? "true" : "false")
                << ",\"tsfActive\":" << (active ? "true" : "false")
                << ",\"tsfCaretIntent\":" << (intent ? "true" : "false")
                << ",\"tsfWordReadyProperty\":" << (GetPropW(foreground, L"OpenKey.TSF.CaretEdit") ? "true" : "false") << "}\n";
        }
        CoUninitialize();
    });
    worker.join(); return 0;
}
