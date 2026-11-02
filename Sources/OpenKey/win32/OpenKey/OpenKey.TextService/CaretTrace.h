#pragma once
#include <windows.h>
#include <cstdint>

// Opt-in, session-local metadata only. No document text, key codes, titles or
// file names. The collector owns the mapping; the DLL never creates it.
namespace OpenKeyCaretTrace {
    constexpr wchar_t Name[] = L"Local\\OpenKey.CaretTrace.v1";
    constexpr LONG Magic = 0x4f4b5401;
    constexpr unsigned Capacity = 128;
    enum Stage : unsigned { Begin, Inactive, ContextMismatch, Disabled, Modifier,
        SettingsDisabled, IntentMissing, Unmapped, ReadLock, Status, Composition,
        Selection, Scope, Clone, LeftShift, RightShift, LeftRead, RightRead,
        ContextReady, WordBoundary, EditRules, ForwardUnsupported, Ready,
        WriteLock, WriteRejected, Applied, Held, CompositionStart, CompositionDenied, CompositionEnd,
        SessionRegister, SessionSelection, SessionLock, SessionUnlock, SessionRetry, SessionDone, SessionIgnored };
    inline const char* Label(unsigned stage) {
        static const char* labels[] = {"begin", "inactive", "context-mismatch", "disabled", "modifier",
            "settings-disabled", "intent-missing", "unmapped", "read-lock", "status", "composition",
            "selection", "input-scope", "clone", "left-shift", "right-shift", "left-read", "right-read",
            "context-ready", "word-boundary", "edit-rules", "forward-unsupported", "ready",
            "write-lock", "write-rejected", "applied", "held", "composition-start", "composition-denied", "composition-end",
            "session-register", "session-selection", "session-lock", "session-unlock", "session-retry", "session-done", "session-ignored"};
        return stage < sizeof(labels) / sizeof(*labels) ? labels[stage] : "unknown";
    }
    struct Record {
        unsigned build = 10, process = 0, thread = 0, kind = 0, stage = Begin;
        std::int32_t hr = 0, before = -1, after = -1, word = -1, mode = -1;
        unsigned flags = 0, eaten = 0, beforeClass = 0, afterClass = 0;
    };
    struct Slot { volatile LONG committed; Record record; };
    struct Buffer { LONG magic; volatile LONG enabled, next; Slot slots[Capacity]; };
    inline void Publish(Record record) {
        HANDLE mapping = OpenFileMappingW(FILE_MAP_WRITE, FALSE, Name);
        if (!mapping) return;
        auto* buffer = static_cast<Buffer*>(MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(Buffer)));
        if (buffer) {
            if (buffer->magic == Magic && buffer->enabled) {
                record.process = GetCurrentProcessId(); record.thread = GetCurrentThreadId();
                const LONG serial = InterlockedIncrement(&buffer->next);
                auto& slot = buffer->slots[static_cast<unsigned>(serial) % Capacity];
                InterlockedExchange(&slot.committed, 0); slot.record = record;
                MemoryBarrier(); InterlockedExchange(&slot.committed, serial);
            }
            UnmapViewOfFile(buffer);
        }
        CloseHandle(mapping);
    }
}
