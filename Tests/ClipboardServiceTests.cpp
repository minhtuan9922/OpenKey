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
constexpr UINT Html = 500;
void require(bool condition) { if (!condition) throw std::runtime_error("Clipboard fixture assertion failed"); }
HWND window() { return active ? reinterpret_cast<HWND>(1) : nullptr; }
BOOL open(HWND owner) { require(owner == reinterpret_cast<HWND>(1)); if (busy) return FALSE; isOpen = true; return TRUE; }
BOOL close() { require(isOpen); isOpen = false; ++closes; return TRUE; }
UINT registerFormat(LPCWSTR) { return Html; }
HANDLE get(UINT format) { require(isOpen); return formats.count(format) ? formats.at(format) : nullptr; }
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
SIZE_T size(HGLOBAL handle) { return blocks.at(handle)->bytes.size(); }
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
#define EmptyClipboard fixture::empty
#define SetClipboardData fixture::set
#define GlobalAlloc fixture::alloc
#define GlobalLock fixture::lock
#define GlobalUnlock fixture::unlock
#define GlobalFree fixture::free
#define GlobalSize fixture::size
#define GetActiveWindow fixture::window
#include "../Sources/OpenKey/win32/OpenKey/OpenKey/ConvertService.cpp"
int main() {
    using namespace fixture; using R = ConvertService::ClipboardResult;
    ConvertService::Options options; options.toAllCaps = true;
    try {
        test("Unicode success", [&] { unicode(L"tiếng việt 😀"); require(ConvertService::convertClipboard(options) == R::Success); require(text() == L"TIẾNG VIỆT 😀" && closes == 1 && empties == 1); });
        test("HTML text success", [&] { html("tiếng việt"); require(ConvertService::convertClipboard(options) == R::Success); require(htmlText() == document("TIẾNG VIỆT") && closes == 1); });
        test("both formats", [&] { unicode(L"việt"); html("việt"); require(ConvertService::convertClipboard(options) == R::Success && formats.size() == 2 && allocations == 2); });
        test("busy", [&] { unicode(L"old"); busy = true; require(ConvertService::convertClipboard(options) == R::ClipboardUnavailable && !closes && !empties && text() == L"old"); });
        test("no active window", [&] { unicode(L"old"); active = false; require(ConvertService::convertClipboard(options) == R::ClipboardUnavailable && !empties && !closes); });
        test("no text", [&] { require(ConvertService::convertClipboard(options) == R::NoText && closes == 1 && !empties); });
        test("non-text format", [&] { const BYTE data = 42; seed(CF_BITMAP, &data, 1); require(ConvertService::convertClipboard(options) == R::NoText && formats.count(CF_BITMAP) && !empties); });
        test("unterminated Unicode", [&] { const wchar_t data[] = { L'a', L'b' }; seed(CF_UNICODETEXT, data, sizeof(data)); require(ConvertService::convertClipboard(options) == R::NoText && !empties && closes == 1); });
        test("unterminated HTML", [&] { const char data[] = { 'a', 'b' }; seed(Html, data, sizeof(data)); require(ConvertService::convertClipboard(options) == R::NoText && !empties); });
        test("read lock failure", [&] { unicode(L"old"); lockFail = 1; require(ConvertService::convertClipboard(options) == R::NoText && !empties && text() == L"old"); });
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
        for (const auto& fragment : { "&unknown; Việt", "<script>Việt</script>", "<STYLE>Việt</STYLE>", "<span title='broken>Việt", "<!--broken" }) test("unsupported HTML preserves data", [&] { unicode(L"old"); html(fragment); const auto original = htmlText(); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old" && htmlText() == original); });
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
        for (const auto& invalid : { "&apos", "&", "&Amp;", "&unknownEntity;", "&#;", "&#x;", "&#-1;", "&#0;", "&#xD800;", "&#xDFFF;", "&#x110000;", "&#999999999999999999999999;", "&#xFFFFFFFFFFFFFFFF;", "&#12;", "&#127;", "&#128;", "&#xFDD0;", "&#xFFFF;", "&#x10FFFF;", "&#12A;", "&#xG1;", "&amp&;" })
            test("HTML invalid entity preserves data", [&] { unicode(L"old"); html(invalid); const auto original = htmlText(); require(ConvertService::convertClipboard(options) == R::WriteFailed && !empties && text() == L"old" && htmlText() == original); });
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
            test("HTML unknown entity still preserves data", [&] { html("xin <b>&unknown;</b>"); const auto original = htmlText(); require(ConvertService::convertClipboard(casing) == R::WriteFailed && !empties && htmlText() == original); });
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
        for (const auto* unsupported : { "&apos", "&NotEqualTilde", "&Copy",
            "&copy&unknown;", "&#169", "&#xA9" })
            test("HTML unsupported recovery preserves data", [&] {
                unicode(L"old"); html(unsupported); const auto original = htmlText();
                require(ConvertService::convertClipboard(options) == R::WriteFailed
                    && !empties && text() == L"old" && htmlText() == original);
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
        std::cout << "Clipboard service: " << passed << "/" << passed << " passed (fake APIs only)\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
