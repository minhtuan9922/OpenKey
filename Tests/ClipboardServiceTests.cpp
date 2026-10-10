#include <windows.h>
#include <map>
#include <memory>
#include <cstring>
#define main RunEngineRegression
#include "EngineRegressionTests.cpp"
#undef main
namespace fixture {
struct Block { std::vector<BYTE> bytes; int locks = 0; bool owned = false; };
std::map<HANDLE, std::unique_ptr<Block>> blocks;
std::map<UINT, HANDLE> formats;
bool active = true, busy = false, emptyFails = false, isOpen = false;
int allocFail = 0, lockFail = 0, allocations = 0, locks = 0, closes = 0, empties = 0, passed = 0;
UINT setFail = 0;
UINT getFail = 0, sizeFail = 0;
bool registerFails = false;
HWND expectedOwner = reinterpret_cast<HWND>(1);
int windowCalls = 0;
constexpr UINT Html = 500;
void require(bool condition) { if (!condition) throw std::runtime_error("Clipboard fixture assertion failed"); }
HWND window() { ++windowCalls; return active ? reinterpret_cast<HWND>(1) : nullptr; }
BOOL open(HWND owner) { require(owner == expectedOwner); if (busy) return FALSE; isOpen = true; return TRUE; }
BOOL close() { require(isOpen); isOpen = false; ++closes; return TRUE; }
UINT registerFormat(LPCWSTR) { return registerFails ? 0 : Html; }
BOOL available(UINT format) { require(isOpen); return formats.count(format) != 0; }
HANDLE get(UINT format) { require(isOpen); return format != getFail && formats.count(format) ? formats.at(format) : nullptr; }
HGLOBAL alloc(UINT flags, SIZE_T size) {
    require(flags == GMEM_MOVEABLE);
    if (++allocations == allocFail) return nullptr;
    auto block = std::make_unique<Block>(); block->bytes.resize(size);
    HANDLE handle = block.get(); blocks.emplace(handle, std::move(block)); return handle;
}
LPVOID lock(HGLOBAL handle) {
    if (++locks == lockFail) return nullptr;
    auto& block = *blocks.at(handle); ++block.locks; return block.bytes.data();
}
BOOL unlock(HGLOBAL handle) { auto& block = *blocks.at(handle); require(block.locks > 0); --block.locks; return TRUE; }
SIZE_T size(HGLOBAL handle) { return sizeFail && formats.count(sizeFail) && formats.at(sizeFail) == handle ? 0 : blocks.at(handle)->bytes.size(); }
HGLOBAL free(HGLOBAL handle) { require(!blocks.at(handle)->owned && !blocks.at(handle)->locks); blocks.erase(handle); return nullptr; }
BOOL empty() {
    require(isOpen); ++empties; if (emptyFails) return FALSE;
    for (const auto& format : formats) { require(!blocks.at(format.second)->locks); blocks.erase(format.second); }
    formats.clear(); return TRUE;
}
HANDLE set(UINT format, HANDLE handle) {
    require(isOpen && !blocks.at(handle)->locks);
    if (setFail == format) return nullptr;
    blocks.at(handle)->owned = true; formats[format] = handle; return handle;
}
void seed(UINT format, const void* data, size_t count) {
    auto block = std::make_unique<Block>(); block->bytes.resize(count);
    std::memcpy(block->bytes.data(), data, count); block->owned = true;
    HANDLE handle = block.get(); blocks.emplace(handle, std::move(block)); formats[format] = handle;
}
void unicode(const std::wstring& value) { seed(CF_UNICODETEXT, value.c_str(), (value.size() + 1) * sizeof(wchar_t)); }
std::string document(const std::string& fragment, const std::string& newline = "\r\n", bool noContext = false,
    bool selection = false, size_t selectionStart = 0, size_t selectionEnd = 0) {
    std::string header = "Version:1.0" + newline;
    for (const auto* key : { "StartHTML:", "EndHTML:", "StartFragment:", "EndFragment:" })
        header += std::string(key) + ((noContext && (std::string(key) == "StartHTML:" || std::string(key) == "EndHTML:")) ? "-1" : "0000000000") + newline;
    if (selection) for (const auto* key : { "StartSelection:", "EndSelection:" }) header += std::string(key) + "0000000000" + newline;
    header += "SourceURL:https://example.test/CaseSensitive" + newline;
    const std::string before = noContext ? "<!--StartFragment-->" : "<html><body><!--StartFragment-->",
        after = noContext ? "<!--EndFragment-->" : "<!--EndFragment--></body></html>";
    const size_t offsets[] = { header.size(), header.size() + before.size() + fragment.size() + after.size(), header.size() + before.size(), header.size() + before.size() + fragment.size() };
    size_t index = 0;
    for (const auto* key : { "StartHTML:", "EndHTML:", "StartFragment:", "EndFragment:" }) {
        const auto position = header.find(key) + std::strlen(key);
        const auto number = std::to_string(offsets[index++]);
        if (noContext && index <= 2) continue;
        header.replace(position, 10, std::string(10 - number.size(), '0') + number);
    }
    if (selection) {
        index = 0;
        for (const auto* key : { "StartSelection:", "EndSelection:" }) {
            const auto number = std::to_string(offsets[2] + (index++ ? selectionEnd : selectionStart));
            header.replace(header.find(key) + std::strlen(key), 10, std::string(10 - number.size(), '0') + number);
        }
    }
    return header + before + fragment + after;
}
void rawHtml(const std::string& value) { seed(Html, value.c_str(), value.size() + 1); }
void html(const std::string& value) { rawHtml(document(value)); }
std::string htmlText() { return reinterpret_cast<const char*>(blocks.at(formats.at(Html))->bytes.data()); }
std::wstring text() { const auto& bytes = blocks.at(formats.at(CF_UNICODETEXT))->bytes; return reinterpret_cast<const wchar_t*>(bytes.data()); }
template<typename Action> void test(const char* name, Action action) {
    blocks.clear(); formats.clear(); active = true; busy = emptyFails = isOpen = false;
    allocFail = lockFail = allocations = locks = closes = empties = 0; setFail = 0;
    windowCalls = 0; expectedOwner = reinterpret_cast<HWND>(1);
    getFail = sizeFail = 0; registerFails = false;
    try {
        action(); require(!isOpen);
        for (const auto& block : blocks) require(block.second->owned && !block.second->locks);
        ++passed;
    } catch (...) { std::cerr << "Failed: " << name << '\n'; throw; }
}
}
#define OpenClipboard fixture::open
#define CloseClipboard fixture::close
#define RegisterClipboardFormatW fixture::registerFormat
#define GetClipboardData fixture::get
#define IsClipboardFormatAvailable fixture::available
#define EmptyClipboard fixture::empty
#define SetClipboardData fixture::set
#define GlobalAlloc fixture::alloc
#define GlobalLock fixture::lock
#define GlobalUnlock fixture::unlock
#define GlobalFree fixture::free
#define GlobalSize fixture::size
#define GetActiveWindow fixture::window
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/ConvertService.cpp"
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/LegacyQuickConvert.h"
int main() {
    using namespace fixture; using R = ConvertService::ClipboardResult;
    ConvertService::Options options; options.toAllCaps = true;
    try {
        const auto setLegacyOptions = [](const ConvertService::Options& value) {
            convertToolFromCode = static_cast<Uint8>(value.fromCode);
            convertToolToCode = static_cast<Uint8>(value.toCode);
            convertToolToAllCaps = value.toAllCaps;
            convertToolToAllNonCaps = value.toAllNonCaps;
            convertToolToCapsFirstLetter = value.toCapsFirstLetter;
            convertToolToCapsEachWord = value.toCapsEachWord;
            convertToolRemoveMark = value.removeMark;
        };
        for (int from = 0; from < 5; ++from) for (int to = 0; to < 5; ++to)
            for (int flags = 0; flags < 32; ++flags) test("legacy quick convert options matrix", [&] {
                ConvertService::Options value; value.fromCode = from; value.toCode = to;
                value.toAllCaps = (flags & 1) != 0; value.toAllNonCaps = (flags & 2) != 0;
                value.toCapsFirstLetter = (flags & 4) != 0; value.toCapsEachWord = (flags & 8) != 0;
                value.removeMark = (flags & 16) != 0;
                ConvertService::Options encode; encode.toCode = from;
                const auto source = ConvertService::convert(L"tiếng Việt. đường ệ", encode);
                const auto expected = ConvertService::convert(source, value);
                setLegacyOptions(value); unicode(source); active = false;
                expectedOwner = reinterpret_cast<HWND>(2);
                require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::Success
                    && text() == expected && windowCalls == 0 && closes == 1 && empties == 1);
                require(convertToolFromCode == from && convertToolToCode == to
                    && convertToolToAllCaps == value.toAllCaps && convertToolToAllNonCaps == value.toAllNonCaps
                    && convertToolToCapsFirstLetter == value.toCapsFirstLetter
                    && convertToolToCapsEachWord == value.toCapsEachWord && convertToolRemoveMark == value.removeMark);
            });
        test("legacy quick convert HTML and selection", [&] {
            setLegacyOptions(options); unicode(L"việt"); active = false;
            expectedOwner = reinterpret_cast<HWND>(2);
            rawHtml(document("<b title='việt'>&unknown; vi&#7879;t</b>", "\r\n", false, true, 16, 25));
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::Success
                && text() == L"VIỆT" && htmlText() == document("<b title='việt'>&amp;UNKNOWN; VIỆT</b>", "\r\n", false, true, 16, 29)
                && windowCalls == 0);
        });
        for (const auto* fragment : { "<script>việt</script>", "<span title='broken>việt" })
            test("legacy quick convert invalid HTML preserves formats", [&] {
                setLegacyOptions(options); unicode(L"old"); html(fragment); const auto original = htmlText();
                require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::WriteFailed
                    && !empties && text() == L"old" && htmlText() == original && windowCalls == 0);
            });
        test("legacy quick convert null owner preserves data", [&] {
            setLegacyOptions(options); unicode(L"old");
            require(LegacyQuickConvert::convertClipboard(nullptr) == R::ClipboardUnavailable
                && !empties && !closes && text() == L"old" && windowCalls == 0);
        });
        test("legacy quick convert busy Clipboard", [&] {
            setLegacyOptions(options); unicode(L"old"); busy = true;
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::ClipboardUnavailable
                && !empties && !closes && text() == L"old");
        });
        test("legacy quick convert no text", [&] {
            setLegacyOptions(options);
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::NoText && !empties && closes == 1);
        });
        for (int failure : { 1, 2 }) test("legacy quick convert allocation failure", [&] {
            setLegacyOptions(options); unicode(L"old"); html("việt"); const auto original = htmlText(); allocFail = failure;
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::WriteFailed
                && !empties && text() == L"old" && htmlText() == original);
        });
        for (bool source : { false, true }) test("legacy quick convert legacy HTML preserves data", [&] {
            auto value = options; if (source) value.fromCode = 1; else value.toCode = 1;
            setLegacyOptions(value); unicode(L"old"); html("việt"); const auto original = htmlText();
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::WriteFailed
                && !empties && text() == L"old" && htmlText() == original);
        });
        test("legacy quick convert invalid code preserves data", [&] {
            auto value = options; value.toCode = 255; setLegacyOptions(value); unicode(L"old");
            require(LegacyQuickConvert::convertClipboard(expectedOwner) == R::WriteFailed
                && !empties && !closes && text() == L"old");
        });
        test("Unicode success", [&] { unicode(L"tiếng việt 😀"); require(ConvertService::convertClipboard(options) == R::Success); require(text() == L"TIẾNG VIỆT 😀" && closes == 1 && empties == 1); });
        test("HTML text success", [&] { html("tiếng việt"); require(ConvertService::convertClipboard(options) == R::Success); require(htmlText() == document("TIẾNG VIỆT") && closes == 1); });
        test("both formats", [&] { unicode(L"việt"); html("việt"); require(ConvertService::convertClipboard(options) == R::Success && formats.size() == 2 && allocations == 2); });
        test("busy", [&] { unicode(L"old"); busy = true; require(ConvertService::convertClipboard(options) == R::ClipboardUnavailable && !closes && !empties && text() == L"old"); });
        test("no active window", [&] { unicode(L"old"); active = false; require(ConvertService::convertClipboard(options) == R::ClipboardUnavailable && !empties && !closes); });
        test("no text", [&] { require(ConvertService::convertClipboard(options) == R::NoText && closes == 1 && !empties); });
        test("non-text format", [&] { const BYTE data = 42; seed(CF_BITMAP, &data, 1); require(ConvertService::convertClipboard(options) == R::NoText && formats.count(CF_BITMAP) && !empties); });
        test("unterminated Unicode", [&] { const wchar_t data[] = { L'a', L'b' }; seed(CF_UNICODETEXT, data, sizeof(data)); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && closes == 1); });
        test("unterminated HTML", [&] { const char data[] = { 'a', 'b' }; seed(Html, data, sizeof(data)); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        test("read lock failure", [&] { unicode(L"old"); lockFail = 1; require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old"); });
        for (bool legacy : { false, true }) for (UINT broken : { UINT(CF_UNICODETEXT), Html })
            for (int failure = 0; failure < 6; ++failure) test("unreadable format aborts both formats before clear", [&] {
                setLegacyOptions(options);
                if (broken == CF_UNICODETEXT && failure >= 3) {
                    const BYTE bytes[] = { 'a', 0, 'b', 0 };
                    seed(broken, bytes, failure == 3 ? sizeof(bytes) : failure == 4 ? 0 : 3);
                } else unicode(L"việt");
                if (broken == Html && failure >= 3) {
                    const char bytes[] = { 'a', 'b', 'c' };
                    seed(broken, bytes, failure == 4 ? 0 : sizeof(bytes));
                } else html("<b>việt</b>");
                if (failure == 0) getFail = broken;
                if (failure == 1) sizeFail = broken;
                if (failure == 2) lockFail = broken == CF_UNICODETEXT ? 1 : 2;
                const auto originalFormats = formats;
                std::map<HANDLE, std::vector<BYTE>> originalBytes;
                for (const auto& block : blocks) originalBytes.emplace(block.first, block.second->bytes);
                const auto result = legacy ? LegacyQuickConvert::convertClipboard(expectedOwner)
                    : ConvertService::convertClipboard(options);
                require(result == R::WriteFailed && !empties && !allocations && closes == 1
                    && formats == originalFormats && blocks.size() == originalBytes.size());
                for (const auto& block : blocks) require(block.second->bytes == originalBytes.at(block.first));
            });
        for (bool legacy : { false, true }) test("format registration failure preserves clipboard", [&] {
            setLegacyOptions(options); unicode(L"việt"); html("việt");
            const auto original = htmlText(); registerFails = true;
            const auto result = legacy ? LegacyQuickConvert::convertClipboard(expectedOwner)
                : ConvertService::convertClipboard(options);
            require(result == R::WriteFailed && !empties && !allocations && closes == 1
                && text() == L"việt" && htmlText() == original && formats.size() == 2);
        });
        for (bool legacy : { false, true }) test("empty Unicode companion is retained", [&] {
            setLegacyOptions(options); unicode(L""); html("việt");
            const auto result = legacy ? LegacyQuickConvert::convertClipboard(expectedOwner)
                : ConvertService::convertClipboard(options);
            require(result == R::Success && formats.size() == 2 && text().empty()
                && htmlText() == document("VIỆT") && allocations == 2 && empties == 1);
        });
        for (bool legacy : { false, true }) test("empty HTML companion is not discarded", [&] {
            setLegacyOptions(options); unicode(L"việt"); rawHtml("");
            const auto result = legacy ? LegacyQuickConvert::convertClipboard(expectedOwner)
                : ConvertService::convertClipboard(options);
            require(result == R::WriteFailed && !empties && !allocations
                && formats.size() == 2 && text() == L"việt" && htmlText().empty());
        });
        for (bool legacy : { false, true }) test("empty text is a no-op", [&] {
            setLegacyOptions(options); unicode(L"");
            const auto result = legacy ? LegacyQuickConvert::convertClipboard(expectedOwner)
                : ConvertService::convertClipboard(options);
            require(result == R::NoText && !empties && !allocations && formats.count(CF_UNICODETEXT));
        });
        for (int failure : { 1, 2 }) test("allocation failure before clear", [&] { unicode(L"old"); html("old"); allocFail = failure; require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && closes == 1 && text() == L"old" && formats.size() == 2); });
        test("output lock failure", [&] { unicode(L"old"); lockFail = 2; require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old"); });
        test("empty failure", [&] { unicode(L"old"); emptyFails = true; require(ConvertService::convertClipboard(options) == R::WriteFailed && text() == L"old" && closes == 1); });
        test("set failure releases memory", [&] { unicode(L"old"); setFail = CF_UNICODETEXT; require(ConvertService::convertClipboard(options) == R::WriteFailed && formats.empty() && blocks.empty() && closes == 1); });
        test("partial write", [&] { unicode(L"old"); html("old"); setFail = Html; require(ConvertService::convertClipboard(options) == R::WriteFailed && formats.size() == 1 && text() == L"OLD" && blocks.size() == 1); });
        test("invalid options preserve data", [&] { unicode(L"old"); auto invalid = options; invalid.toCode = 5; require(ConvertService::convertClipboard(invalid) == R::WriteFailed && !empties && !closes && text() == L"old"); });
        test("HTML byte offsets shrink", [&] { html("<B Class='Việt > Test'>tiếng Việt 😀</B>"); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success); require(htmlText() == document("<B Class='Việt > Test'>TIENG VIET 😀</B>")); });
        test("HTML comments preserved", [&] { html("<!--Giữ Nguyên >-->việt"); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("<!--Giữ Nguyên >-->VIỆT")); });
        for (const auto& newline : { std::string("\n"), std::string("\r") }) test("HTML newline variants", [&] { rawHtml(document("việt", newline)); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("VIỆT", newline)); });
        test("HTML empty fragment", [&] { html(""); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("")); });
        for (const auto& fragment : { "<script>Việt</script>", "<STYLE>Việt</STYLE>", "<span title='broken>Việt", "<!--broken" }) test("unsupported HTML preserves data", [&] { unicode(L"old"); html(fragment); const auto original = htmlText(); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old" && htmlText() == original); });
        test("HTML invalid UTF8", [&] { html(std::string("bad\xFF")); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        test("HTML malformed header", [&] { unicode(L"old"); rawHtml("not CF_HTML"); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old"); });
        test("HTML out of range offset", [&] { auto value = document("việt"); value.replace(value.find("EndFragment:") + 12, 10, "9999999999"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        test("HTML duplicate field", [&] { auto value = document("việt"); value.replace(value.find("SourceURL:"), 10, "StartHTML:"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        test("HTML selection malformed", [&] { auto value = document("việt"); value.replace(value.find("SourceURL:"), 10, "StartSelection:"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        for (bool sourceCode : { false, true }) test("HTML legacy encoding preserves data", [&] { html("việt"); auto unsupported = options; if (sourceCode) unsupported.fromCode = 1; else unsupported.toCode = 1; require(ConvertService::convertClipboard(unsupported) == R::WriteFailed && !empties); });
        for (const auto& newline : { std::string("\r\n"), std::string("\n"), std::string("\r") }) test("HTML no context", [&] { rawHtml(document("<b>việt 😀</b>", newline, true)); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("<b>VIỆT 😀</b>", newline, true)); });
        for (bool noContext : { false, true }) {
            test("HTML selection shrinks with text", [&] { const std::string original = "<b>tiếng Việt 😀</b>", expected = "<b>TIENG VIET 😀</b>"; rawHtml(document(original, "\r\n", noContext, true, 3, original.size() - 4)); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success && htmlText() == document(expected, "\r\n", noContext, true, 3, expected.size() - 4)); });
            test("HTML selection within text", [&] { rawHtml(document("<b>tiếng Việt</b>", "\r\n", noContext, true, std::string("<b>tiếng ").size(), std::string("<b>tiếng Việt").size())); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success && htmlText() == document("<b>TIENG VIET</b>", "\r\n", noContext, true, 9, 13)); });
            test("HTML selection across nodes", [&] { rawHtml(document("<b>Việt</b><i>Đẹp</i>", "\r\n", noContext, true, 3, std::string("<b>Việt</b><i>Đẹp").size())); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success && htmlText() == document("<b>VIET</b><i>DEP</i>", "\r\n", noContext, true, 3, 17)); });
            test("HTML zero length selection", [&] { rawHtml(document("Việt", "\r\n", noContext, true, 0, 0)); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("VIỆT", "\r\n", noContext, true, 0, 0)); });
            test("HTML empty selected fragment", [&] { rawHtml(document("", "\r\n", noContext, true)); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document("", "\r\n", noContext, true)); });
        }
        for (int malformed : { 0, 1, 2, 3, 4, 5 }) test("HTML invalid selection preserves data", [&] {
            auto value = document("<b>Việt 😀</b>", "\r\n", false, true, 3, 9);
            if (malformed == 0) value.replace(value.find("StartSelection:"), 14, "OtherSelection");
            if (malformed == 1) value.replace(value.find("EndSelection:") + 13, 10, "0000000000");
            if (malformed == 2) value.replace(value.find("EndSelection:") + 13, 10, "9999999999");
            if (malformed == 3) value = document("<b>Việt 😀</b>", "\r\n", false, true, 8, 3);
            if (malformed == 4) value = document("<b>Việt 😀</b>", "\r\n", false, true, 6, 9);
            if (malformed == 5) value = document("<b>Việt 😀</b>", "\r\n", false, true, 3, 11);
            unicode(L"old"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old" && htmlText() == value);
        });
        test("HTML no context mismatched sentinel", [&] { auto value = document("việt", "\r\n", true); value.replace(value.find("EndHTML:") + 8, 2, "00"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        test("HTML no context marker mismatch", [&] { auto value = document("việt", "\r\n", true); value.replace(value.find("<!--EndFragment-->"), 18, "<!--WrongMarker-->"); rawHtml(value); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&amp; Việt", "&amp; VIỆT" },
            { "&AMP;&LT;&GT;&QUOT;", "&amp;&lt;&gt;\"" }, { "&lt;việt&gt;", "&lt;VIỆT&gt;" },
            { "&quot;việt&apos;", "\"VIỆT'" }, { "Việt&nbsp;nam", "VIỆT NAM" },
            { "vi&#7879;t", "VIỆT" }, { "vi&#x1ec7;t", "VIỆT" }, { "vi&#X0001EC7;t", "VIỆT" },
            { "&#128512; &#x1F600;", "😀 😀" }, { "&#60;script&#62;", "&lt;SCRIPT&gt;" },
            { "&#38;lt;", "&amp;LT;" }, { "a > b", "A &gt; B" }, { "&#9;&#10;&#13;", "\t\n&#13;" },
            { "a\r\nb", "A\nB" }, { "a\rb", "A\nB" }, { "a&#13;\nb", "A&#13;\nB" },
            { "<b title='&amp; Việt'>vi&#7879;t</b>", "<b title='&amp; Việt'>VIỆT</b>" } })
            test("HTML entity conversion", [&] { html(pair.first); require(ConvertService::convertClipboard(options) == R::Success && htmlText() == document(pair.second)); });
        test("HTML entity remove marks", [&] { html("vi&#7879;t &#x111;ẹp"); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success && htmlText() == document("VIET DEP")); });
        test("HTML entities lowercase", [&] { html("VI&#x1EC6;T &AMP;"); auto lower = options; lower.toAllCaps = false; lower.toAllNonCaps = true; require(ConvertService::convertClipboard(lower) == R::Success && htmlText() == document("việt &amp;")); });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&apos", "&amp;APOS" },
            { "&", "&amp;" }, { "&Amp;", "&amp;AMP;" }, { "&unknownEntity;", "&amp;UNKNOWNENTITY;" },
            { "&#;", "&amp;#;" }, { "&#x;", "&amp;#X;" }, { "&#-1;", "&amp;#-1;" },
            { "&#xG1;", "&amp;#XG1;" }, { "&amp&;", "&amp;&amp;;" },
            { "&unknown; Việt", "&amp;UNKNOWN; VIỆT" }, { "&&copy;", "&amp;©" },
            { "&amp;unknown;", "&amp;UNKNOWN;" }, { "& Việt", "&amp; VIỆT" } })
            test("HTML literal reference recovery integration", [&] {
                html(pair.first);
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document(pair.second));
            });
        for (const auto& source : { std::string("&unknownEntity;"), std::string("&#xG1;"),
            std::string("&#;"), std::string("&apos"), std::string("&") }) {
            for (size_t offset = 0; offset <= source.size(); ++offset)
                for (bool noContext : { false, true }) test("HTML literal reference selection", [&] {
                    const auto expected = HtmlClipboard::escapeText(wideToUtf8(
                        ConvertService::convert(utf8ToWide(source), options)));
                    const auto prefix = HtmlClipboard::escapeText(wideToUtf8(
                        ConvertService::convert(utf8ToWide(source.substr(0, offset)), options)));
                    rawHtml(document(source, "\r\n", noContext, true, offset, offset));
                    require(ConvertService::convertClipboard(options) == R::Success
                        && htmlText() == document(expected, "\r\n", noContext, true, prefix.size(), prefix.size()));
                });
        }
        for (const auto& pair : { std::pair<std::string, std::string>{ "&unknown;", "&unknown;" },
            { "&", "&" }, { "&#", "&#" }, { "&#x", "&#x" }, { "&#X;", "&#X;" },
            { "&#+1;", "&#+1;" }, { "&#x-1;", "&#x-1;" }, { "&#xG1;", "&#xG1;" },
            { "&&copy;", "&©" }, { "&amp&;", "&&;" }, { "&#&amp;", "&#&" },
            { "&amp;copy;", "&copy;" }, { "&unknown;&lt;", "&unknown;<" },
            { "&\r\nViệt", "&\nViệt" }, { "&é;", "&é;" } })
            test("HTML literal reference golden and escape roundtrip", [&] {
                std::string decoded, roundtrip;
                require(HtmlClipboard::decodeText(pair.first, decoded) && decoded == pair.second);
                require(HtmlClipboard::decodeText(HtmlClipboard::escapeText(decoded), roundtrip)
                    && roundtrip == decoded);
            });
        test("HTML long unknown name is bounded and literal", [&] {
            const std::string source = "&" + std::string(10000, 'z') + ";";
            std::string decoded;
            require(HtmlClipboard::decodeText(source, decoded) && decoded == source);
            html(source); require(ConvertService::convertClipboard(options) == R::Success
                && htmlText() == document("&amp;" + std::string(10000, 'Z') + ";"));
        });
        test("HTML recovered reference invalid UTF8 preserves data", [&] {
            unicode(L"old"); html("&unknown;\xFF"); const auto original = htmlText();
            require(ConvertService::convertClipboard(options) == R::WriteFailed
                && !empties && text() == L"old" && htmlText() == original);
        });
        test("HTML recovered reference allocation failure preserves data", [&] {
            unicode(L"old"); html("&unknown;"); const auto original = htmlText(); allocFail = true;
            require(ConvertService::convertClipboard(options) == R::WriteFailed
                && !empties && text() == L"old" && htmlText() == original);
        });
        for (bool noContext : { false, true }) test("HTML entity selection mapping", [&] { const std::string original = "<b>vi&#7879;t &amp; nam</b>", expected = "<b>VIET &amp; NAM</b>"; rawHtml(document(original, "\r\n", noContext, true, std::string("<b>vi&#7879;t ").size(), original.size() - 4)); auto remove = options; remove.removeMark = true; require(ConvertService::convertClipboard(remove) == R::Success && htmlText() == document(expected, "\r\n", noContext, true, 8, expected.size() - 4)); });
        for (size_t offset = 1; offset < std::string("&#7879;").size(); ++offset) test("HTML selection inside numeric entity", [&] { rawHtml(document("&#7879;", "\r\n", false, true, offset, std::string("&#7879;").size())); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        for (size_t offset = 1; offset < std::string("&amp;").size(); ++offset) test("HTML selection inside named entity", [&] { rawHtml(document("&amp;", "\r\n", false, true, 0, offset)); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties); });
        for (bool words : { false, true }) {
            auto casing = options; casing.toAllCaps = false; casing.toCapsEachWord = words; casing.toCapsFirstLetter = !words;
            test("HTML casing across inline tags", [&] { html("<b>tiếng</b> <i>VIỆT</i>. <u>xin</u> CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "<b>Tiếng</b> <i>Việt</i>. <u>Xin</u> Chào" : "<b>Tiếng</b> <i>việt</i>. <u>Xin</u> chào")); });
            test("HTML casing across split word", [&] { html("<b>tiế</b><i>ng</i> VIỆT"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "<b>Tiế</b><i>ng</i> Việt" : "<b>Tiế</b><i>ng</i> việt")); });
            test("HTML comments do not reset casing", [&] { html("x<!--Giữ Nguyên-->IN CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "X<!--Giữ Nguyên-->in Chào" : "X<!--Giữ Nguyên-->in chào")); });
            test("HTML sentence punctuation split nodes", [&] { html("xin<span>.</span><i> </i>CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("Xin<span>.</span><i> </i>Chào")); });
            test("HTML entities participate in casing", [&] { html("vi&#7879;t <b>nAM</b>"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "Việt <b>Nam</b>" : "Việt <b>nam</b>")); });
            test("HTML quotes and attributes unchanged", [&] { html("<span Class='VIỆT > TEST'>xin</span> CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "<span Class='VIỆT > TEST'>Xin</span> Chào" : "<span Class='VIỆT > TEST'>Xin</span> chào")); });
            test("HTML linebreak boundary", [&] { html("xin<br/>CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("Xin<br/>Chào")); });
            test("HTML paragraph boundary", [&] { html("<P>xin</P><div>CHÀO</div>"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("<P>Xin</P><div>Chào</div>")); });
            test("HTML literal linebreak casing", [&] { html("xin\r\n<i>CHÀO</i>"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("Xin\n<i>Chào</i>")); });
            test("HTML unknown entity participates in casing", [&] { html("xin <b>&unknown;</b>"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("Xin <b>&amp;unknown;</b>")); });
            for (bool noContext : { false, true }) test("HTML selection preview does not advance casing", [&] {
                const std::string original = "<b>TIẾNG</b> vi&#7879;t <i>NAM</i>", expected = words ? "<b>Tiếng</b> Việt <i>Nam</i>" : "<b>Tiếng</b> việt <i>nam</i>";
                rawHtml(document(original, "\r\n", noContext, true, 3, original.size() - 4));
                require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(expected, "\r\n", noContext, true, 3, expected.size() - 4));
            });
            test("HTML selection within first word", [&] { rawHtml(document("vi&#7879;t NAM", "\r\n", false, true, 1, 2)); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "Việt Nam" : "Việt nam", "\r\n", false, true, 1, 2)); });
            test("HTML repeated calls reset state", [&] { html("xin CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success); html("xin CHÀO"); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document(words ? "Xin Chào" : "Xin chào")); });
        }
        test("HTML stateful casing empty fragment", [&] { auto casing = options; casing.toAllCaps = false; casing.toCapsEachWord = true; html(""); require(ConvertService::convertClipboard(casing) == R::Success && htmlText() == document("")); });
        // Exhaustive lookup/UTF-8 test: Windows encodes expected scalars, not
        // appendUtf8 (the implementation under test). Goldens below separately
        // protect representative mappings and integration behavior.
        test("HTML named table coverage", [&] {
            require(HtmlClipboard::NamedEntities::entries.size() == 2125);
            size_t doubleScalars = 0;
            for (const auto& entry : HtmlClipboard::NamedEntities::entries)
                if (entry.second) ++doubleScalars;
            require(doubleScalars == 93);
        });
        for (const auto& entry : HtmlClipboard::NamedEntities::entries)
            test("HTML all named references", [&] {
                std::wstring wide;
                for (const auto scalar : { entry.first, entry.second }) {
                    if (!scalar) continue;
                    if (scalar <= 0xFFFFu) wide += static_cast<wchar_t>(scalar);
                    else {
                        wide += static_cast<wchar_t>(0xD800u + ((scalar - 0x10000u) >> 10));
                        wide += static_cast<wchar_t>(0xDC00u + ((scalar - 0x10000u) & 0x3FFu));
                    }
                }
                const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                    wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
                require(size > 0);
                std::string expected(size, '\0'), decoded;
                require(WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wide.data(),
                    static_cast<int>(wide.size()), expected.data(), size, nullptr, nullptr) == size);
                require(HtmlClipboard::decodeText("&" + std::string(entry.name) + ";", decoded)
                    && decoded == expected);
            });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&eacute;&Eacute;", "éÉ" },
            { "&NotEqualTilde;", "\u2242\u0338" }, { "&acE;", "\u223E\u0333" },
            { "&Afr;", "\U0001D504" }, { "&CounterClockwiseContourIntegral;", "\u2233" },
            { "&Tab;&NewLine;", "\t\n" }, { "&lt;&LT;&gt;&GT;", "<<>>" },
            { "&not;&notin;", "\u00AC\u2209" }, { "&fjlig;", "fj" },
            { "&amp;eacute;", "&eacute;" } })
            test("HTML named reference golden", [&] {
                std::string decoded;
                require(HtmlClipboard::decodeText(pair.first, decoded) && decoded == pair.second);
            });
        test("HTML extended names integration", [&] {
            html("<b title='&eacute;'>vi&ecirc;t &NotEqualTilde; &Afr; &lt;</b>");
            require(ConvertService::convertClipboard(options) == R::Success
                && htmlText() == document("<b title='&eacute;'>VIÊT \u2242\u0338 \U0001D504 &lt;</b>"));
        });
        for (bool noContext : { false, true }) test("HTML two-scalar selection mapping", [&] {
            const std::string source = "<b>&NotEqualTilde; việt</b>",
                expected = "<b>\u2242\u0338 VIỆT</b>";
            rawHtml(document(source, "\r\n", noContext, true, 3, std::string("<b>&NotEqualTilde;").size()));
            require(ConvertService::convertClipboard(options) == R::Success
                && htmlText() == document(expected, "\r\n", noContext, true, 3, std::string("<b>\u2242\u0338").size()));
        });
        for (size_t offset = 1; offset < std::string("&NotEqualTilde;").size(); ++offset)
            test("HTML selection inside two-scalar name preserves data", [&] {
                unicode(L"old"); const auto original = document("&NotEqualTilde;", "\r\n", false, true, 0, offset);
                rawHtml(original);
                require(ConvertService::convertClipboard(options) == R::WriteFailed
                    && !empties && text() == L"old" && htmlText() == original);
            });
        for (bool words : { false, true }) test("HTML extended names stateful casing", [&] {
            auto casing = options; casing.toAllCaps = false;
            casing.toCapsEachWord = words; casing.toCapsFirstLetter = !words;
            html("&dstrok;<i>ẸP</i> &ecirc;M");
            require(ConvertService::convertClipboard(casing) == R::Success
                && htmlText() == document(words ? "Đ<i>ẹp</i> Êm" : "Đ<i>ẹp</i> êm"));
        });
        test("HTML legacy alias table", [&] {
            require(HtmlClipboard::NamedEntities::validLegacyNames());
            require(HtmlClipboard::NamedEntities::maximumNameLength == 31);
        });
        for (const auto name : HtmlClipboard::NamedEntities::legacyNames)
            test("HTML every legacy alias at EOF and before text", [&] {
                const auto source = "&" + std::string(name);
                std::string expected, actual;
                require(HtmlClipboard::decodeText(source + ";", expected));
                require(HtmlClipboard::decodeText(source, actual) && actual == expected);
                require(HtmlClipboard::decodeText(source + " Việt", actual) && actual == expected + " Việt");
                require(HtmlClipboard::decodeText(source + "X", actual) && actual == expected + "X");
            });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&copy Việt", "© Việt" },
            { "&notit;", "¬it;" }, { "&notin;", "∉" }, { "&notin", "¬in" },
            { "&notinva;", "∉" }, { "&amp=hello", "&=hello" }, { "&ampere;", "&ere;" },
            { "&copycat", "©cat" }, { "&COPYING", "©ING" }, { "&LTbr&GT", "<br>" },
            { "&amp;copy", "&copy" }, { "&copy &eacute;", "© é" },
            { "&frac12x", "½x" }, { "&nbsp\r\nViệt", " \nViệt" } })
            test("HTML legacy longest-match golden", [&] {
                std::string actual;
                require(HtmlClipboard::decodeText(pair.first, actual) && actual == pair.second);
            });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&apos", "&amp;APOS" },
            { "&NotEqualTilde", "&amp;NOTEQUALTILDE" }, { "&Copy", "&amp;COPY" },
            { "&copy&unknown;", "©&amp;UNKNOWN;" } })
            test("HTML unmatched reference recovery", [&] {
                html(pair.first);
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document(pair.second));
            });
        test("HTML legacy aliases integration and attributes", [&] {
            html("<b title='&notit; &copy=Việt'>&copy việt &amp=nam &notit;</b>");
            require(ConvertService::convertClipboard(options) == R::Success
                && htmlText() == document("<b title='&notit; &copy=Việt'>© VIỆT &amp;=NAM ¬IT;</b>"));
        });
        for (bool noContext : { false, true }) {
            test("HTML legacy alias selection mapping", [&] {
                const std::string source = "<b>&copy việt</b>", expected = "<b>© VIỆT</b>";
                rawHtml(document(source, "\r\n", noContext, true, 3, 8));
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document(expected, "\r\n", noContext, true, 3, 5));
            });
            test("HTML longest-match selection prefix is not an entity", [&] {
                unicode(L"old"); const auto original = document("&notin;", "\r\n", noContext, true, 0, 4);
                rawHtml(original);
                require(ConvertService::convertClipboard(options) == R::WriteFailed
                    && !empties && text() == L"old" && htmlText() == original);
            });
            test("HTML selection after recovered prefix", [&] {
                rawHtml(document("&notit;", "\r\n", noContext, true, 0, 4));
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document("¬IT;", "\r\n", noContext, true, 0, 2));
            });
        }
        for (const auto* source : { "&copy", "&copy;", "&notin;", "&notinva;" })
            for (size_t offset = 1; offset < std::strlen(source); ++offset)
                test("HTML selection inside recovered/full reference", [&] {
                    rawHtml(document(source, "\r\n", false, true, 0, offset));
                    require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties);
                });
        for (bool words : { false, true }) test("HTML missing-semicolon aliases preserve casing state", [&] {
            auto casing = options; casing.toAllCaps = false;
            casing.toCapsEachWord = words; casing.toCapsFirstLetter = !words;
            html("&ecirc<i>M</i> VIỆT");
            require(ConvertService::convertClipboard(casing) == R::Success
                && htmlText() == document(words ? "Ê<i>m</i> Việt" : "Ê<i>m</i> việt"));
        });
        test("HTML preview cannot split CRLF normalization", [&] {
            rawHtml(document("a\r\nb", "\r\n", false, true, 0, 2));
            require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties);
        });
        for (const auto& pair : { std::pair<std::string, std::string>{ "&#0;", "�" },
            { "&#xD800;", "�" }, { "&#xDFFF;", "�" }, { "&#x110000;", "�" },
            { "&#999999999999999999999999;", "�" }, { "&#xFFFFFFFFFFFFFFFF;", "�" },
            { "&#128;", "€" }, { "&#169", "©" }, { "&#xA9", "©" },
            { "&#X0001F600", "😀" }, { "&#12A;", "\fA;" }, { "&#65=hi", "A=hi" },
            { "&#x41Z", "AZ" }, { "&#x41F", "П" }, { "&#65;&#x42; &#67", "AB C" },
            { "&#38lt;", "&lt;" }, { "&#60script&#62", "<script>" },
            { "&#13\n", "\r\n" }, { "&#0việt", "�việt" }, { "&#128việt", "€việt" } })
            test(pair.first.c_str(), [&] {
                std::string decoded;
                require(HtmlClipboard::decodeText(pair.first, decoded) && decoded == pair.second);
                html(pair.first);
                require(ConvertService::convertClipboard(ConvertService::Options{}) == R::Success
                    && htmlText() == document(HtmlClipboard::escapeText(wideToUtf8(
                        ConvertService::convert(utf8ToWide(pair.second), ConvertService::Options{})))));
            });
        // Independent C1 oracle: Windows' CP1252 decoder, not the HTML table.
        for (unsigned int number = 0x80; number <= 0x9F; ++number)
            for (bool hex : { false, true }) for (bool terminated : { false, true })
                test("HTML all C1 replacements and unmapped controls", [&] {
                    const char byte = static_cast<char>(number);
                    wchar_t wide;
                    require(MultiByteToWideChar(1252, 0, &byte, 1, &wide, 1) == 1);
                    char expected[4]{};
                    const int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
                        &wide, 1, expected, sizeof(expected), nullptr, nullptr);
                    require(count > 0);
                    const char digits[] = "0123456789ABCDEF";
                    const std::string value = hex ? std::string(1, digits[number >> 4]) + digits[number & 15]
                        : std::to_string(number);
                    const auto source = std::string(hex ? "&#x" : "&#") + value + (terminated ? ";" : "");
                    std::string decoded;
                    require(HtmlClipboard::decodeText(source, decoded) && decoded == std::string(expected, count));
                    html(source);
                    require(ConvertService::convertClipboard(ConvertService::Options{}) == R::Success
                        && htmlText() == document(wideToUtf8(ConvertService::convert(
                            std::wstring(1, wide), ConvertService::Options{}))));
                });
        for (unsigned int number : { 1u, 9u, 10u, 12u, 13u, 31u, 127u,
            0xD7FFu, 0xE000u, 0xFDD0u, 0xFDEFu, 0xFFFEu, 0xFFFFu, 0x10FFFFu })
            test("HTML controls/noncharacters retain scalar", [&] {
                std::string expected, decoded; HtmlClipboard::appendUtf8(number, expected);
                const auto source = "&#" + std::to_string(number) + ";";
                require(HtmlClipboard::decodeText(source, decoded) && decoded == expected);
                html(source);
                require(ConvertService::convertClipboard(ConvertService::Options{}) == R::Success
                    && htmlText() == document(HtmlClipboard::escapeText(expected)));
            });
        for (const auto& digits : { std::string(10000, '9'), std::string(10000, 'F') })
            test("HTML numeric saturation consumes all digits", [&] {
                const auto source = std::string(digits[0] == 'F' ? "&#x" : "&#") + digits + "; Việt";
                std::string decoded;
                require(HtmlClipboard::decodeText(source, decoded) && decoded == "� Việt");
                html(source);
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document("� VIỆT"));
            });
        test("HTML very long leading zeroes", [&] {
            std::string decoded;
            require(HtmlClipboard::decodeText("&#" + std::string(10000, '0') + "128;", decoded)
                && decoded == "€");
        });
        for (bool noContext : { false, true }) {
            for (const auto* source : { "&#128; Việt", "&#128 Việt", "&#0; Việt", "&#xD800 Việt" })
                test("HTML recovered numeric selection maps bytes", [&] {
                    const std::string original(source);
                    const size_t end = original.find(' ');
                    rawHtml(document(original, "\r\n", noContext, true, 0, end));
                    const std::string expected = original.find("128") != std::string::npos ? "€ VIỆT" : "� VIỆT";
                    require(ConvertService::convertClipboard(options) == R::Success
                        && htmlText() == document(expected, "\r\n", noContext, true, 0, 3));
                });
            test("HTML numeric selection uses full digit run", [&] {
                unicode(L"old"); const auto original = document("&#1285", "\r\n", noContext, true, 0, 5);
                rawHtml(original);
                require(ConvertService::convertClipboard(options) == R::WriteFailed
                    && !empties && text() == L"old" && htmlText() == original);
            });
            test("HTML numeric selection before non-digit suffix", [&] {
                rawHtml(document("&#128x", "\r\n", noContext, true, 0, 5));
                require(ConvertService::convertClipboard(options) == R::Success
                    && htmlText() == document("€X", "\r\n", noContext, true, 0, 3));
            });
        }
        for (const auto* source : { "&#128", "&#128;", "&#xD800", "&#x110000;" })
            for (size_t offset = 1; offset < std::strlen(source); ++offset)
                test("HTML selection inside numeric recovery preserves data", [&] {
                    unicode(L"old"); const auto original = document(source, "\r\n", false, true, 0, offset);
                    rawHtml(original);
                    require(ConvertService::convertClipboard(options) == R::WriteFailed
                        && !empties && text() == L"old" && htmlText() == original);
                });
        test("HTML numeric recovery attributes untouched", [&] {
            html("<b title='&#0; &#128'>vi&#7879t &#128</b>");
            require(ConvertService::convertClipboard(options) == R::Success
                && htmlText() == document("<b title='&#0; &#128'>VIỆT €</b>"));
        });
        for (bool words : { false, true }) test("HTML numeric recovery stateful casing", [&] {
            auto casing = options; casing.toAllCaps = false;
            casing.toCapsEachWord = words; casing.toCapsFirstLetter = !words;
            html("vi&#7879t <b>NAM</b>");
            require(ConvertService::convertClipboard(casing) == R::Success
                && htmlText() == document(words ? "Việt <b>Nam</b>" : "Việt <b>nam</b>"));
        });
        std::cout << "Clipboard service: " << passed << "/" << passed << " passed (fake APIs only)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
