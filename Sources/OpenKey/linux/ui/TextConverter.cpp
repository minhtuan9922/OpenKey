#include "TextConverter.h"
#include "ConvertTool.h"

#include <iterator>
#include <QFutureWatcher>
#include <QtConcurrentRun>
#include <mutex>

namespace {
std::mutex legacyConversionMutex; // Legacy ConvertTool options are process globals.
int encodingIndex(const QString& name) {
    return QStringList{"unicode", "tcvn3", "vni", "compound", "cp1258"}.indexOf(name);
}

// Each six-character group is a Vietnamese vowel and its five tones.
// Uppercase is explicit: never derive this mapping from case conversion.
constexpr char16_t lower[] =
    u"aàáảãạăằắẳẵặâầấẩẫậeèéẻẽẹêềếểễệiìíỉĩị"
    u"oòóỏõọôồốổỗộơờớởỡợuùúủũụưừứửữựyỳýỷỹỵđ";
constexpr char16_t upper[] =
    u"AÀÁẢÃẠĂẰẮẲẴẶÂẦẤẨẪẬEÈÉẺẼẸÊỀẾỂỄỆIÌÍỈĨỊ"
    u"OÒÓỎÕỌÔỒỐỔỖỘƠỜỚỞỠỢUÙÚỦŨỤƯỪỨỬỮỰYỲÝỶỸỴĐ";
constexpr char16_t lowerBase[] =
    u"aaaaaa" u"aaaaaa" u"aaaaaa" u"eeeeee" u"eeeeee" u"iiiiii"
    u"oooooo" u"oooooo" u"oooooo" u"uuuuuu" u"uuuuuu" u"yyyyyy" u"d";
constexpr char16_t upperBase[] =
    u"AAAAAA" u"AAAAAA" u"AAAAAA" u"EEEEEE" u"EEEEEE" u"IIIIII"
    u"OOOOOO" u"OOOOOO" u"OOOOOO" u"UUUUUU" u"UUUUUU" u"YYYYYY" u"D";
static_assert(std::size(lower) == 74); // 12 vowels * 6 tones, plus d-stroke and terminator.
static_assert(std::size(lower) == std::size(upper));
static_assert(std::size(lower) == std::size(lowerBase));
static_assert(std::size(upper) == std::size(upperBase));

bool validText(const QString& text) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar unit = text.at(i);
        if (unit.isNull() || unit.isLowSurrogate()) return false;
        if (unit.isHighSurrogate()) {
            if (++i == text.size() || !text.at(i).isLowSurrogate()) return false;
        }
    }
    return true;
}

void removeMarks(QString& text, const std::atomic_bool& cancelled) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        if ((i % 1024) == 0 && cancelled.load()) return;
        const auto unit = text.at(i).unicode();
        for (std::size_t j = 0; j + 1 < std::size(lower); ++j) {
            if (unit == lower[j]) {
                text[i] = QChar(lowerBase[j]);
                break;
            }
            if (unit == upper[j]) {
                text[i] = QChar(upperBase[j]);
                break;
            }
        }
    }
}
}

TextConverter::TextConverter(QObject* parent, qsizetype maxInput, qsizetype maxOutput)
    : QObject(parent), maxInput_(maxInput), maxOutput_(maxOutput) {}

TextConverter::~TextConverter() {
    if (cancellation_) cancellation_->store(true);
    // The worker only owns text/options/token, never this QObject. Watcher
    // destruction disconnects callbacks without waiting for Qt normalization.
}

void TextConverter::finish(const QString& state, const QString& result) {
    state_ = state;
    result_ = result;
    emit changed();
}

void TextConverter::clear() {
    if (cancellation_) cancellation_->store(true);
    finish(QStringLiteral("idle"));
}

void TextConverter::cancel() {
    if (!busy_ || !cancellation_ || cancellation_->exchange(true)) return;
    finish(QStringLiteral("cancelled"));
}

void TextConverter::convert(const QString& input, const QString& normalization,
                            const QString& letterCase, bool removeVietnameseMarks) {
    convertInternal(input, normalization, letterCase, removeVietnameseMarks, "unicode", "unicode", false);
}
void TextConverter::convertLegacy(const QString& input, const QString& source, const QString& target) {
    convertInternal(input, "none", "keep", false, source, target, true);
}
void TextConverter::convertInternal(const QString& input, const QString& normalization,
    const QString& letterCase, bool removeVietnameseMarks, const QString& source, const QString& target, bool legacy) {
    // Cancel/clear invalidate immediately, but keep the slot occupied until
    // the previous worker retires: repeated clicks cannot create a job queue.
    if (busy_) return;
    if ((normalization != "none" && normalization != "nfc" && normalization != "nfd") ||
        (letterCase != "keep" && letterCase != "upper" && letterCase != "lower") ||
        (legacy && (encodingIndex(source) < 0 || encodingIndex(target) < 0))) {
        finish(QStringLiteral("invalid"));
        return;
    }
    if (input.size() > maxInput_) {
        finish(QStringLiteral("too-large"));
        return;
    }
    if (!validText(input)) {
        finish(QStringLiteral("invalid"));
        return;
    }
    if (input.isEmpty()) {
        finish(QStringLiteral("empty"));
        return;
    }

    struct Outcome { QString state; QString text; };
    const auto token = std::make_shared<std::atomic_bool>(false);
    cancellation_ = token;
    busy_ = true;
    auto* watcher = new QFutureWatcher<Outcome>(this);
    connect(watcher, &QFutureWatcher<Outcome>::finished, this, [this, watcher, token] {
        const auto outcome = watcher->result();
        watcher->deleteLater();
        busy_ = false;
        cancellation_.reset();
        if (!token->load()) finish(outcome.state, outcome.text);
        else emit changed(); // Preserve idle/cancelled, never republish stale text.
    });
    watcher->setFuture(QtConcurrent::run([input, normalization, letterCase, removeVietnameseMarks, source, target, legacy, token, limit = maxOutput_]() -> Outcome {
        if (token->load()) return {};
        QString output = input;
        if (legacy) {
            std::lock_guard<std::mutex> lock(legacyConversionMutex);
            if (token->load()) return {};
            convertToolFromCode = static_cast<Uint8>(source == "compound" ? 0 : encodingIndex(source));
            convertToolToCode = static_cast<Uint8>(target == "compound" ? 0 : encodingIndex(target));
            convertToolToAllCaps = convertToolToAllNonCaps = false;
            convertToolToCapsFirstLetter = convertToolToCapsEachWord = convertToolRemoveMark = false;
            try {
                const auto canonical = source == "compound" ? input.normalized(QString::NormalizationForm_C) : input;
                output = QString::fromUtf8(convertUtil(canonical.toUtf8().toStdString()));
                if (target == "compound") output = output.normalized(QString::NormalizationForm_D);
            }
            catch (const std::range_error&) { return {QStringLiteral("invalid"), {}}; }
        }
        // Qt's individual normalization/case calls cannot be interrupted.
        // Check cancellation between phases; never touch UI from this thread.
        if (removeVietnameseMarks) {
            output = output.normalized(QString::NormalizationForm_C);
            if (token->load()) return {};
            removeMarks(output, *token);
        }
        if (token->load()) return {};
        if (letterCase == "upper") output = output.toUpper();
        else if (letterCase == "lower") output = output.toLower();
        if (token->load()) return {};
        if (normalization == "nfc") output = output.normalized(QString::NormalizationForm_C);
        else if (normalization == "nfd") output = output.normalized(QString::NormalizationForm_D);
        if (token->load()) return {};
        if (output.size() > limit) return {QStringLiteral("too-large"), {}};
        if (!validText(output)) return {QStringLiteral("invalid"), {}};
        return {QStringLiteral("converted"), output};
    }));
    finish(QStringLiteral("running"));
}
