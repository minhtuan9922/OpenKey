#include "ConvertService.h"

#include <windows.h>
#include "HtmlClipboard.h"
#include <climits>

#include "../../../engine/Vietnamese.h"

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <memory>
#include <vector>

namespace {
	constexpr int CodeTableCount = 5;

	bool findKeyCode(const Uint32 characterCode, const int codeTable, int& keyCode, int& markIndex) {
		for (auto iterator = _codeTable[codeTable].begin(); iterator != _codeTable[codeTable].end(); ++iterator) {
			for (size_t index = 0; index < iterator->second.size(); ++index) {
				if (characterCode == iterator->second[index]) {
					keyCode = static_cast<int>(iterator->first);
					markIndex = static_cast<int>(index);
					return true;
				}
			}
		}
		return false;
	}

	Uint16 compoundMarkIndex(const Uint16 mark) {
		for (int index = 0; index < 5; ++index) {
			if (mark == _unicodeCompoundMark[index]) {
				return static_cast<Uint16>((index + 1) << 13);
			}
		}
		return 0;
	}

	std::wstring utf8ToWide(const std::string& value) {
		if (value.empty()) {
			return {};
		}
		const int size = MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0);
		if (size <= 0) {
			return {};
		}
		std::wstring result(static_cast<size_t>(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size);
		return result;
	}

	std::string wideToUtf8(const std::wstring& value) {
		if (value.empty()) {
			return {};
		}
		const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
			nullptr, 0, nullptr, nullptr);
		if (size <= 0) {
			return {};
		}
		std::string result(static_cast<size_t>(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
			result.data(), size, nullptr, nullptr);
		return result;
	}

	struct ClipboardCloser { ~ClipboardCloser() { CloseClipboard(); } };
	struct MemoryDeleter { void operator()(void* memory) const { GlobalFree(memory); } };
	using ClipboardMemory = std::unique_ptr<void, MemoryDeleter>;

	template<typename Character>
	std::basic_string<Character> readClipboardText(const UINT format) {
		const HANDLE memory = GetClipboardData(format);
		if (!memory) return {};
		const size_t count = GlobalSize(memory) / sizeof(Character);
		const auto* text = static_cast<const Character*>(GlobalLock(memory));
		if (!text) return {};
		struct Unlocker { HANDLE memory; ~Unlocker() { GlobalUnlock(memory); } } unlocker{ memory };
		const auto* end = std::find(text, text + count, Character{});
		return end == text + count ? std::basic_string<Character>{} : std::basic_string<Character>(text, end);
	}

	ClipboardMemory copyClipboardBytes(const void* data, const size_t size) {
		ClipboardMemory memory(GlobalAlloc(GMEM_MOVEABLE, size));
		if (!memory) return {};
		void* destination = GlobalLock(memory.get());
		if (destination == nullptr) {
			return {};
		}
		memcpy(destination, data, size);
		GlobalUnlock(memory.get());
		return memory;
	}
}

bool ConvertService::isValidCodeTable(const int codeTable) {
	return codeTable >= 0 && codeTable < CodeTableCount;
}

namespace {
struct ConversionState {
	bool afterSentenceBreak = false;
	bool shouldUpperCase = false;
};

std::wstring convertText(const std::wstring& source, const ConvertService::Options& options, ConversionState& state) {
	if (!ConvertService::isValidCodeTable(options.fromCode) || !ConvertService::isValidCodeTable(options.toCode)) {
		return {};
	}

	std::vector<wchar_t> output;
	output.reserve(source.size() + 1);
	bool& afterSentenceBreak = state.afterSentenceBreak;
	bool& shouldUpperCase = state.shouldUpperCase;

	for (size_t index = 0; index < source.size(); ++index) {
		Uint16 sourceCode = static_cast<Uint16>(source[index]);
		int consumedCharacters = 0;
		int keyCode = 0;
		int markIndex = 0;

		if (index + 1 < source.size()) {
			if (options.fromCode == 2 || options.fromCode == 4) {
				sourceCode = static_cast<Uint16>(source[index]) |
					(static_cast<Uint16>(source[index + 1]) << 8);
				consumedCharacters = 1;
			} else if (options.fromCode == 3) {
				const Uint16 mark = compoundMarkIndex(static_cast<Uint16>(source[index + 1]));
				if (mark != 0) {
					sourceCode = static_cast<Uint16>(source[index]) | mark;
					consumedCharacters = 1;
				}
			}

			if (findKeyCode(sourceCode, options.fromCode, keyCode, markIndex)) {
				index += consumedCharacters;
			} else {
				consumedCharacters = 0;
			}
		}

		if (consumedCharacters == 0) {
			sourceCode = static_cast<Uint16>(source[index]);
		}

		if (findKeyCode(sourceCode, options.fromCode, keyCode, markIndex)) {
			Uint16 target = _codeTable[options.toCode][keyCode][markIndex];
			if ((options.toAllCaps || shouldUpperCase) && markIndex % 2 != 0) {
				target = _codeTable[options.toCode][keyCode][markIndex - 1];
			} else if ((options.toAllNonCaps || !shouldUpperCase) && markIndex % 2 == 0) {
				target = _codeTable[options.toCode][keyCode][markIndex + 1];
			}

			if (options.removeMark) {
				target = keyCodeToCharacter(static_cast<Uint8>(keyCode));
				if (options.toAllCaps) {
					target = static_cast<Uint16>(std::towupper(target));
				} else if (options.toAllNonCaps) {
					target = static_cast<Uint16>(std::towlower(target));
				}
			}

			if (options.toCode == 0 || options.toCode == 1) {
				output.push_back(static_cast<wchar_t>(target));
			} else if (options.toCode == 2 || options.toCode == 4) {
				output.push_back(static_cast<wchar_t>(target & 0xFF));
				if (((target >> 8) & 0xFF) > 32) {
					output.push_back(static_cast<wchar_t>(target >> 8));
				}
			} else {
				if ((target >> 13) > 0) {
					output.push_back(static_cast<wchar_t>(target & 0x1FFF));
					output.push_back(static_cast<wchar_t>(_unicodeCompoundMark[(target >> 13) - 1]));
				} else {
					output.push_back(static_cast<wchar_t>(target));
				}
			}

			shouldUpperCase = false;
			afterSentenceBreak = false;
			continue;
		}

		const wchar_t character = source[index];
		if (options.toAllCaps || shouldUpperCase) {
			output.push_back(std::towupper(character));
		} else if (options.toAllNonCaps || !shouldUpperCase) {
			output.push_back(std::towlower(character));
		} else {
			output.push_back(character);
		}

		if (character == L'\n' || (afterSentenceBreak && character == L' ')) {
			shouldUpperCase = options.toCapsFirstLetter || options.toCapsEachWord;
		} else if (character == L' ' && options.toCapsEachWord) {
			shouldUpperCase = true;
		} else if (character == L'.' || character == L'?' || character == L'!') {
			afterSentenceBreak = true;
		} else {
			shouldUpperCase = false;
			afterSentenceBreak = false;
		}
	}

	return std::wstring(output.begin(), output.end());
}
}

std::wstring ConvertService::convert(const std::wstring& source, const Options& options) {
	ConversionState state{ false, options.toCapsFirstLetter || options.toCapsEachWord };
	return convertText(source, options, state);
}

ConvertService::ClipboardResult ConvertService::convertClipboard(const Options& options) {
	if (!isValidCodeTable(options.fromCode) || !isValidCodeTable(options.toCode)) {
		return ClipboardResult::WriteFailed;
	}
	// This synchronous service is called on the UI thread; never use another app's foreground HWND.
	const HWND owner = GetActiveWindow();
	if (!owner || !OpenClipboard(owner)) {
		return ClipboardResult::ClipboardUnavailable;
	}
	const ClipboardCloser closer;

	const UINT htmlFormat = RegisterClipboardFormatW(L"HTML Format");
	const auto unicodeText = readClipboardText<wchar_t>(CF_UNICODETEXT);
	const auto htmlText = htmlFormat ? readClipboardText<char>(htmlFormat) : std::string{};

	if (unicodeText.empty() && htmlText.empty()) {
		return ClipboardResult::NoText;
	}

	const std::wstring convertedUnicode = unicodeText.empty() ? std::wstring{} : convert(unicodeText, options);
	std::string convertedHtml;
	if (!htmlText.empty()) {
		// CF_HTML remains Unicode, independent of the plain-text code table.
		if (htmlText.size() > INT_MAX || !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, htmlText.data(), static_cast<int>(htmlText.size()), nullptr, 0) ||
			options.fromCode != 0 || options.toCode != 0 ||
			!HtmlClipboard::rewrite(htmlText, convertedHtml,
				[options, state = ConversionState{ false, options.toCapsFirstLetter || options.toCapsEachWord }](const std::string& text, std::string& result) mutable {
				if (!text.empty() && (text.size() > INT_MAX ||
					!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0))) return false;
				result = wideToUtf8(convertText(utf8ToWide(text), options, state));
				return text.empty() || !result.empty();
			})) return ClipboardResult::WriteFailed;
	}
	ClipboardMemory htmlMemory;
	ClipboardMemory unicodeMemory;
	if (!htmlText.empty()) {
		htmlMemory = copyClipboardBytes(convertedHtml.c_str(), convertedHtml.size() + 1);
		if (!htmlMemory) return ClipboardResult::WriteFailed;
	}
	if (!unicodeText.empty()) {
		unicodeMemory = copyClipboardBytes(convertedUnicode.c_str(), (convertedUnicode.size() + 1) * sizeof(wchar_t));
		if (!unicodeMemory) return ClipboardResult::WriteFailed;
	}
	// Allocation/locking failures must leave the original clipboard untouched.
	if (!EmptyClipboard()) {
		return ClipboardResult::WriteFailed;
	}

	bool written = false;
	bool succeeded = true;
	if (htmlMemory) {
		if (SetClipboardData(htmlFormat, htmlMemory.get())) htmlMemory.release();
		else succeeded = false;
		written = true;
	}
	if (unicodeMemory) {
		if (SetClipboardData(CF_UNICODETEXT, unicodeMemory.get())) unicodeMemory.release();
		else succeeded = false;
		written = true;
	}

	return written && succeeded ? ClipboardResult::Success : ClipboardResult::WriteFailed;
}
