#pragma once

#include <string>

class ConvertService {
public:
	enum class ClipboardResult {
		Success,
		NoText,
		ClipboardUnavailable,
		WriteFailed
	};

	struct Options {
		int fromCode = 0;
		int toCode = 0;
		bool toAllCaps = false;
		bool toAllNonCaps = false;
		bool toCapsFirstLetter = false;
		bool toCapsEachWord = false;
		bool removeMark = false;
	};

	static bool isValidCodeTable(int codeTable);
	static std::wstring convert(const std::wstring& source, const Options& options);
	static ClipboardResult convertClipboard(const Options& options);
};
