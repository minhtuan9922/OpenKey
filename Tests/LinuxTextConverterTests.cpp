#include "TextConverter.h"

#include <QMetaMethod>
#include <QMetaProperty>
#include <QFutureWatcher>
#include <QPointer>
#include <QRunnable>
#include <QSemaphore>
#include <QSignalSpy>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QtTest>
#include <atomic>
#include <memory>

namespace {
constexpr qsizetype inputLimit = 64 * 1024;
constexpr qsizetype outputLimit = 256 * 1024;
static_assert(TextConverter::inputLimit == inputLimit);
static_assert(TextConverter::outputLimit == outputLimit);

// Keep the Unicode fixtures concise while exercising the real async pipeline.
class TestConverter : public TextConverter {
public:
    using TextConverter::TextConverter;
    void convert(const QString& input, const QString& normalization,
                 const QString& letterCase, bool removeVietnameseMarks) {
        TextConverter::convert(input, normalization, letterCase, removeVietnameseMarks);
        QTRY_VERIFY_WITH_TIMEOUT(!busy(), 30000);
    }
};

// The worker owns its semaphores, so an early assertion failure can safely
// release it without waiting on the GUI thread or leaving a dangling capture.
class PoolGate {
public:
    PoolGate() : previousMaximum(pool->maxThreadCount()) {
        pool->setMaxThreadCount(1);
        pool->start(QRunnable::create([state = semaphores] {
            state->started.release();
            state->release.acquire();
        }));
    }
    ~PoolGate() {
        release();
        pool->setMaxThreadCount(previousMaximum);
    }
    bool started() const { return semaphores->started.available() != 0; }
    void release() {
        if (!released) {
            released = true;
            semaphores->release.release();
        }
    }
private:
    struct Semaphores { QSemaphore started, release; };
    QThreadPool* pool = QThreadPool::globalInstance();
    const int previousMaximum;
    std::shared_ptr<Semaphores> semaphores = std::make_shared<Semaphores>();
    bool released = false;
};

struct Vowel {
    const char* lower;
    const char* upper;
    char lowerBase;
    char upperBase;
};
// Independent, grouped fixtures cover every Vietnamese vowel/tone and d-stroke.
const Vowel letters[] = {
    {"aàáảãạ", "AÀÁẢÃẠ", 'a', 'A'},
    {"ăằắẳẵặ", "ĂẰẮẲẴẶ", 'a', 'A'},
    {"âầấẩẫậ", "ÂẦẤẨẪẬ", 'a', 'A'},
    {"eèéẻẽẹ", "EÈÉẺẼẸ", 'e', 'E'},
    {"êềếểễệ", "ÊỀẾỂỄỆ", 'e', 'E'},
    {"iìíỉĩị", "IÌÍỈĨỊ", 'i', 'I'},
    {"oòóỏõọ", "OÒÓỎÕỌ", 'o', 'O'},
    {"ôồốổỗộ", "ÔỒỐỔỖỘ", 'o', 'O'},
    {"ơờớởỡợ", "ƠỜỚỞỠỢ", 'o', 'O'},
    {"uùúủũụ", "UÙÚỦŨỤ", 'u', 'U'},
    {"ưừứửữự", "ƯỪỨỬỮỰ", 'u', 'U'},
    {"yỳýỷỹỵ", "YỲÝỶỸỴ", 'y', 'Y'},
    {"đ", "Đ", 'd', 'D'}
};

QString units(std::initializer_list<ushort> values) {
    QString text;
    for (const auto value : values) text.append(QChar(value));
    return text;
}
}

class LinuxTextConverterTests : public QObject {
    Q_OBJECT
private slots:
    void initialPropertiesAndInvokables() {
        QObject parent;
        TextConverter converter(&parent);
        QCOMPARE(converter.parent(), &parent);
        QCOMPARE(converter.state(), QStringLiteral("idle"));
        QVERIFY(converter.result().isEmpty());
        QVERIFY(!converter.busy());
        const auto* meta = converter.metaObject();
        for (const auto* name : {"state", "result"}) {
            const int index = meta->indexOfProperty(name);
            QVERIFY(index >= 0);
            const auto property = meta->property(index);
            QCOMPARE(property.metaType(), QMetaType::fromType<QString>());
            QVERIFY(property.isReadable());
            QVERIFY(!property.isWritable());
            QCOMPARE(property.notifySignal().methodSignature(), QByteArray("changed()"));
        }
        const int busyIndex = meta->indexOfProperty("busy");
        QVERIFY(busyIndex >= 0);
        const auto busyProperty = meta->property(busyIndex);
        QCOMPARE(busyProperty.metaType(), QMetaType::fromType<bool>());
        QVERIFY(busyProperty.isReadable());
        QVERIFY(!busyProperty.isWritable());
        QCOMPARE(busyProperty.notifySignal().methodSignature(), QByteArray("changed()"));
        QSignalSpy changed(&converter, &TextConverter::changed);
        QVERIFY(changed.isValid());
        QVERIFY(QMetaObject::invokeMethod(&converter, "convert", Qt::DirectConnection,
            Q_ARG(QString, QStringLiteral("Đắk Lắk")), Q_ARG(QString, QStringLiteral("none")),
            Q_ARG(QString, QStringLiteral("keep")), Q_ARG(bool, true)));
        QCOMPARE(converter.state(), QStringLiteral("running"));
        QVERIFY(converter.busy());
        QVERIFY(converter.result().isEmpty());
        QCOMPARE(changed.size(), 1);
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), QStringLiteral("Dak Lak"));
        QCOMPARE(changed.size(), 2);
        QVERIFY(QMetaObject::invokeMethod(&converter, "clear", Qt::DirectConnection));
        QCOMPARE(converter.state(), QStringLiteral("idle"));
        QVERIFY(converter.result().isEmpty());
        QCOMPARE(changed.size(), 3);
    }

    void everyVietnameseLetter_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("expected");
        QTest::addColumn<QString>("oppositeCase");
        QTest::addColumn<QString>("caseOption");
        for (int i = 0; i < 13; ++i) {
            for (bool uppercase : {false, true}) {
                const auto& letter = letters[i];
                const QString text = QString::fromUtf8(uppercase ? letter.upper : letter.lower);
                const QString opposite = QString::fromUtf8(uppercase ? letter.lower : letter.upper);
                const QString expected(text.size(), QLatin1Char(uppercase ? letter.upperBase : letter.lowerBase));
                for (bool decomposed : {false, true}) {
                    const QByteArray name = QByteArray::number(i) + (uppercase ? "-upper" : "-lower") +
                                            (decomposed ? "-nfd" : "-nfc");
                    QTest::newRow(name.constData())
                        << (decomposed ? text.normalized(QString::NormalizationForm_D) : text)
                        << expected << opposite << QString::fromLatin1(uppercase ? "lower" : "upper");
                }
            }
        }
    }
    void everyVietnameseLetter() {
        QFETCH(QString, input);
        QFETCH(QString, expected);
        QFETCH(QString, oppositeCase);
        QFETCH(QString, caseOption);
        TestConverter converter;
        for (const auto* normalization : {"none", "nfc", "nfd"}) {
            converter.convert(input, QString::fromLatin1(normalization), "keep", true);
            QCOMPARE(converter.state(), QStringLiteral("converted"));
            QCOMPARE(converter.result(), expected);
        }
        converter.convert(input, "nfc", caseOption, false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), oppositeCase);
        converter.convert(input, "nfd", caseOption, true);
        QCOMPARE(converter.result(), caseOption == "upper" ? expected.toUpper() : expected.toLower());
    }

    void normalizationAndPipeline() {
        TestConverter converter;
        const QString composed = QStringLiteral("Ắ ộ ñ ü ① ﬃ 각");
        const QString decomposed = QStringLiteral("A\u0306\u0301 o\u0323\u0302 n\u0303 u\u0308 ① ﬃ \u1100\u1161\u11a8");
        converter.convert(decomposed, "nfc", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), composed);
        converter.convert(composed, "nfd", "keep", false);
        QCOMPARE(converter.result(), decomposed);
        converter.convert(decomposed, "none", "keep", false);
        QCOMPARE(converter.result(), decomposed);
        converter.convert(decomposed, "none", "keep", true);
        QCOMPARE(converter.result(), QStringLiteral("A o ñ ü ① ﬃ 각"));
        converter.convert(decomposed, "nfd", "upper", true);
        QCOMPARE(converter.result(), QStringLiteral("A O N\u0303 U\u0308 ① FFI \u1100\u1161\u11a8"));
        // Final normalization must also apply to marks in non-Vietnamese text.
        converter.convert(QStringLiteral("ü"), "nfd", "upper", false);
        QCOMPARE(converter.result(), QStringLiteral("U\u0308"));
    }

    void unicodeCase() {
        TestConverter converter;
        converter.convert(QStringLiteral("Straße åäö ø æ σ ж 中文 😀"), "none", "upper", false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), QStringLiteral("STRASSE ÅÄÖ Ø Æ Σ Ж 中文 😀"));
        converter.convert(QStringLiteral("ẞ ÅÄÖ Ø Æ Σ Ж 中文 😀"), "none", "lower", false);
        QCOMPARE(converter.result(), QStringLiteral("ß åäö ø æ σ ж 中文 😀"));
        converter.convert(QStringLiteral("đẶng ß"), "nfc", "upper", true);
        QCOMPARE(converter.result(), QStringLiteral("DANG SS"));
        converter.convert(QStringLiteral("ĐẶNG ẞ"), "nfc", "lower", true);
        QCOMPARE(converter.result(), QStringLiteral("dang ß"));
    }

    void preservesOtherLettersAndMarks() {
        TestConverter converter;
        const QString text = QStringLiteral("ñ ü ä å ç ø ł ß ï ą ě ǿ ά ΐ Ж 中 😀 q\u0301 \u0301\u0308\n\t!?—");
        converter.convert(text, "none", "keep", true);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), text);
        // Extra, unrelated marks on a Vietnamese vowel must survive the mapping.
        converter.convert(QStringLiteral("a\u0306\u0301\u0338 Đ\u0308"), "none", "keep", true);
        QCOMPARE(converter.result(), QStringLiteral("a\u0338 D\u0308"));
        converter.convert(QStringLiteral("n\u0303 u\u0308 q\u0301"), "none", "keep", true);
        QCOMPARE(converter.result(), QStringLiteral("ñ ü q\u0301"));
    }

    void identityAndWhitespace() {
        TestConverter converter;
        const QString text = QStringLiteral("Đắk a\u0306\u0301 ñ\r\n\t Ελληνικά 中文 👩‍💻 🏳️‍🌈 𝄞 e\u0301\u0308 !");
        converter.convert(text, "none", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), text);
        QCOMPARE(converter.result().toUtf8(), text.toUtf8());
        const QString whitespace = QStringLiteral(" \t\n\r\u00a0\u2003");
        converter.convert(whitespace, "nfc", "upper", true);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), whitespace);
        // Supplementary characters remain a valid pair at either boundary.
        const QString pair = units({0xd83d, 0xde00});
        converter.convert(pair + "a" + pair, "nfd", "upper", true);
        QCOMPARE(converter.result(), pair + "A" + pair);
    }

    void invalidText_data() {
        QTest::addColumn<QString>("input");
        QTest::newRow("high-only") << units({0xd800});
        QTest::newRow("low-only") << units({0xdfff});
        QTest::newRow("high-before-letter") << units({0xd800, 'a'});
        QTest::newRow("low-in-middle") << units({'a', 0xdc00, 'b'});
        QTest::newRow("high-at-end") << units({'a', 0xdbff});
        QTest::newRow("high-high-low") << units({0xd800, 0xd801, 0xdc00});
        QTest::newRow("low-high") << units({0xdc00, 0xd800});
        QTest::newRow("pair-extra-low") << units({0xd800, 0xdc00, 0xdc01});
        QTest::newRow("pair-extra-high") << units({0xd800, 0xdc00, 0xd801});
        QTest::newRow("nul-only") << units({0});
        QTest::newRow("nul-leading") << units({0, 'a'});
        QTest::newRow("nul-middle") << units({'a', 0, 'b'});
        QTest::newRow("nul-trailing") << units({'a', 0});
    }
    void invalidText() {
        QFETCH(QString, input);
        TestConverter converter;
        for (const auto* normalization : {"none", "nfc", "nfd"}) {
            for (bool remove : {false, true}) {
                converter.convert("previous result", "none", "keep", false);
                QSignalSpy changed(&converter, &TextConverter::changed);
                converter.convert(input, QString::fromLatin1(normalization), "keep", remove);
                QCOMPARE(converter.state(), QStringLiteral("invalid"));
                QVERIFY(converter.result().isEmpty());
                QCOMPARE(changed.size(), 1);
                QVERIFY(!converter.busy());
            }
        }
    }

    void invalidOptions_data() {
        QTest::addColumn<QString>("normalization");
        QTest::addColumn<QString>("letterCase");
        for (const auto* option : {"", "NFC", "NFD", "None", "nfkc", "nfkd", " nfc", "nfc ", "garbage"})
            QTest::newRow((QByteArray("normalization-") + option).constData()) << QString::fromLatin1(option) << QStringLiteral("keep");
        for (const auto* option : {"", "UPPER", "Lower", "Keep", "title", "fold", " upper", "lower ", "garbage"})
            QTest::newRow((QByteArray("case-") + option).constData()) << QStringLiteral("none") << QString::fromLatin1(option);
        QTest::newRow("nul-option") << units({'n', 'f', 'c', 0}) << QStringLiteral("keep");
        QTest::newRow("surrogate-option") << QStringLiteral("none") << units({0xd800});
    }
    void invalidOptions() {
        QFETCH(QString, normalization);
        QFETCH(QString, letterCase);
        TestConverter converter;
        for (const QString& input : {QStringLiteral("text"), QString()}) {
            converter.convert("stale", "none", "keep", false);
            converter.convert(input, normalization, letterCase, true);
            QCOMPARE(converter.state(), QStringLiteral("invalid"));
            QVERIFY(converter.result().isEmpty());
        }
    }

    void limits() {
        TestConverter converter;
        const QString ascii(inputLimit, QLatin1Char('a'));
        converter.convert(ascii, "none", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), ascii);
        converter.convert(ascii + 'a', "none", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("too-large"));
        QVERIFY(converter.result().isEmpty());

        const QString emoji = QStringLiteral("😀").repeated(inputLimit / 2);
        QCOMPARE(emoji.size(), inputLimit);
        converter.convert(emoji, "nfd", "keep", true);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), emoji);
        converter.convert(emoji + QStringLiteral("😀"), "none", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("too-large"));
        QVERIFY(converter.result().isEmpty());

        // A canonical four-unit decomposition reaches the output limit exactly.
        const QString greek(inputLimit, QChar(0x1f82));
        converter.convert(greek, "nfd", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result().size(), outputLimit);
        QCOMPARE(converter.result(), QStringLiteral("\u03b1\u0313\u0300\u0345").repeated(inputLimit));
        // Input limit is enforced before a conversion that would shrink it.
        converter.convert(QString(inputLimit + 1, QChar(0x1eaf)), "none", "keep", true);
        QCOMPARE(converter.state(), QStringLiteral("too-large"));
        QVERIFY(converter.result().isEmpty());
    }

    void synchronousRejections_data() {
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("normalization");
        QTest::addColumn<QString>("letterCase");
        QTest::addColumn<QString>("expectedState");
        QTest::newRow("empty") << QString() << QStringLiteral("none")
            << QStringLiteral("keep") << QStringLiteral("empty");
        QTest::newRow("invalid-option") << QStringLiteral("text") << QStringLiteral("NFC")
            << QStringLiteral("keep") << QStringLiteral("invalid");
        QTest::newRow("invalid-case") << QStringLiteral("text") << QStringLiteral("none")
            << QStringLiteral("KEEP") << QStringLiteral("invalid");
        QTest::newRow("invalid-text") << units({0xd800}) << QStringLiteral("none")
            << QStringLiteral("keep") << QStringLiteral("invalid");
        QTest::newRow("oversize") << QString(inputLimit + 1, QLatin1Char('a'))
            << QStringLiteral("none") << QStringLiteral("keep") << QStringLiteral("too-large");
    }
    void synchronousRejections() {
        QFETCH(QString, input);
        QFETCH(QString, normalization);
        QFETCH(QString, letterCase);
        QFETCH(QString, expectedState);
        TextConverter converter;
        converter.convert("old result", "none", "keep", false);
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        QCOMPARE(converter.result(), QStringLiteral("old result"));
        PoolGate gate;
        QTRY_VERIFY_WITH_TIMEOUT(gate.started(), 30000);
        QSignalSpy changed(&converter, &TextConverter::changed);
        converter.convert(input, normalization, letterCase, false);
        QCOMPARE(converter.state(), expectedState);
        QVERIFY(converter.result().isEmpty());
        QVERIFY(!converter.busy());
        QCOMPARE(changed.size(), 1);
    }

    void asyncRunningClearsOldResult() {
        TextConverter converter;
        converter.convert("old result", "none", "keep", false);
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        QCOMPARE(converter.result(), QStringLiteral("old result"));
        PoolGate gate;
        QTRY_VERIFY_WITH_TIMEOUT(gate.started(), 30000);
        QSignalSpy changed(&converter, &TextConverter::changed);
        QStringList observedStates, observedResults;
        QList<bool> observedBusy;
        std::atomic_bool allSignalsOnOwnerThread{true};
        std::atomic_int directSignalCount{0};
        auto* const ownerThread = converter.thread();
        QCOMPARE(ownerThread, QCoreApplication::instance()->thread());
        // A direct connection checks the emitting thread, even if an erroneous
        // worker emission would otherwise be queued to the test's thread.
        connect(&converter, &TextConverter::changed, this, [&] {
            if (QThread::currentThread() != ownerThread)
                allSignalsOnOwnerThread.store(false);
            directSignalCount.fetch_add(1);
        }, Qt::DirectConnection);
        connect(&converter, &TextConverter::changed, this, [&] {
            observedStates.append(converter.state());
            observedResults.append(converter.result());
            observedBusy.append(converter.busy());
        });
        converter.convert(QStringLiteral("Đắk Lắk"), "none", "upper", true);
        QCOMPARE(converter.state(), QStringLiteral("running"));
        QVERIFY(converter.busy());
        QVERIFY(converter.result().isEmpty());
        QCOMPARE(changed.size(), 1);
        bool timerFired = false;
        QTimer timer;
        timer.setSingleShot(true);
        connect(&timer, &QTimer::timeout, this, [&] { timerFired = true; });
        timer.start(0);
        // The GUI event loop must service timers while conversion is pending.
        // The gate keeps the worker queued until after this assertion.
        QTRY_VERIFY_WITH_TIMEOUT(timerFired, 30000);
        QCOMPARE(converter.state(), QStringLiteral("running"));
        QVERIFY(converter.busy());
        QCOMPARE(changed.size(), 1);
        QVERIFY(allSignalsOnOwnerThread.load());
        QCOMPARE(directSignalCount.load(), 1);
        gate.release();
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), QStringLiteral("DAK LAK"));
        QCOMPARE(changed.size(), 2);
        QCOMPARE(observedStates, QStringList({"running", "converted"}));
        QCOMPARE(observedResults, QStringList({QString(), "DAK LAK"}));
        QCOMPARE(observedBusy, QList<bool>({true, false}));
        QVERIFY(allSignalsOnOwnerThread.load());
        QCOMPARE(directSignalCount.load(), 2);
    }

    void busyRejectionAndStaleCompletion_data() {
        QTest::addColumn<QString>("action");
        QTest::addColumn<QString>("expectedState");
        QTest::newRow("running") << QString() << QStringLiteral("running");
        QTest::newRow("cancel") << QStringLiteral("cancel") << QStringLiteral("cancelled");
        QTest::newRow("clear") << QStringLiteral("clear") << QStringLiteral("idle");
    }
    void busyRejectionAndStaleCompletion() {
        QFETCH(QString, action);
        QFETCH(QString, expectedState);
        PoolGate gate;
        QTRY_VERIFY_WITH_TIMEOUT(gate.started(), 30000);
        TextConverter converter;
        QSignalSpy changed(&converter, &TextConverter::changed);
        converter.convert(QStringLiteral("Đắk Lắk"), "none", "keep", true);
        QCOMPARE(converter.state(), QStringLiteral("running"));
        QVERIFY(converter.busy());
        QCOMPARE(changed.size(), 1);
        QCOMPARE(converter.findChildren<QFutureWatcherBase*>().size(), 1);
        if (action == "cancel") {
            QVERIFY(QMetaObject::invokeMethod(&converter, "cancel", Qt::DirectConnection));
        } else if (action == "clear") {
            converter.clear();
        }
        QCOMPARE(converter.state(), expectedState);
        QVERIFY(converter.result().isEmpty());
        QVERIFY(converter.busy());
        QCOMPARE(changed.size(), action.isEmpty() ? 1 : 2);
        const auto signalCount = changed.size();
        // Repeated valid and invalid submissions must not replace the state,
        // emit notifications, or allocate another watcher/job while busy.
        for (int i = 0; i < 100; ++i) {
            converter.convert("replacement", "nfd", "upper", false);
            converter.convert({}, "none", "keep", false);
            converter.convert("invalid option", "bad", "keep", false);
            converter.convert(units({0xd800}), "none", "keep", false);
        }
        converter.convert(QString(inputLimit + 1, QLatin1Char('a')), "none", "keep", false);
        QCoreApplication::processEvents();
        QCOMPARE(converter.state(), expectedState);
        QVERIFY(converter.result().isEmpty());
        QVERIFY(converter.busy());
        QCOMPARE(changed.size(), signalCount);
        QCOMPARE(converter.findChildren<QFutureWatcherBase*>().size(), 1);
        gate.release();
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        if (action.isEmpty()) {
            QCOMPARE(converter.state(), QStringLiteral("converted"));
            QCOMPARE(converter.result(), QStringLiteral("Dak Lak"));
            QCOMPARE(changed.size(), 2);
        } else {
            QCOMPARE(converter.state(), expectedState);
            QVERIFY(converter.result().isEmpty());
        }
        // Retirement permits a fresh request after cancellation or clear.
        converter.convert("fresh", "none", "upper", false);
        QCOMPARE(converter.state(), QStringLiteral("running"));
        QVERIFY(converter.busy());
        QTRY_VERIFY_WITH_TIMEOUT(!converter.busy(), 30000);
        QCOMPARE(converter.state(), QStringLiteral("converted"));
        QCOMPARE(converter.result(), QStringLiteral("FRESH"));
    }

    void destructionWhilePending() {
        PoolGate gate;
        QTRY_VERIFY_WITH_TIMEOUT(gate.started(), 30000);
        auto converter = std::make_unique<TextConverter>();
        QPointer<TextConverter> guarded(converter.get());
        converter->convert(QStringLiteral("Đắk Lắk"), "nfd", "upper", true);
        QVERIFY(converter->busy());
        QCOMPARE(converter->state(), QStringLiteral("running"));
        // Destruction must return while the queued worker cannot start.
        converter.reset();
        QVERIFY(guarded.isNull());
        auto drained = std::make_shared<QSemaphore>();
        QThreadPool::globalInstance()->start(QRunnable::create([drained] { drained->release(); }));
        gate.release();
        QTRY_VERIFY_WITH_TIMEOUT(drained->available() != 0, 30000);
        QCoreApplication::processEvents();
        TextConverter survivor;
        survivor.convert("survives", "none", "upper", false);
        QTRY_VERIFY_WITH_TIMEOUT(!survivor.busy(), 30000);
        QCOMPARE(survivor.state(), QStringLiteral("converted"));
        QCOMPARE(survivor.result(), QStringLiteral("SURVIVES"));
    }

    void emptyClearAndNotifications() {
        TestConverter converter;
        QSignalSpy changed(&converter, &TextConverter::changed);
        // Signal observers see the complete new state and result together.
        QString observedState, observedResult;
        connect(&converter, &TextConverter::changed, this, [&] {
            observedState = converter.state();
            observedResult = converter.result();
        });
        converter.convert("stale", "none", "keep", false);
        QCOMPARE(observedState, QStringLiteral("converted"));
        QCOMPARE(observedResult, QStringLiteral("stale"));
        converter.convert({}, "none", "keep", false);
        QCOMPARE(converter.state(), QStringLiteral("empty"));
        QVERIFY(converter.result().isEmpty());
        QCOMPARE(observedState, converter.state());
        QCOMPARE(observedResult, converter.result());
        converter.clear();
        QCOMPARE(converter.state(), QStringLiteral("idle"));
        QVERIFY(converter.result().isEmpty());
        converter.convert("Đ", "none", "keep", true);
        QCOMPARE(converter.result(), QStringLiteral("D"));
        converter.clear();
        QCOMPARE(converter.state(), QStringLiteral("idle"));
        QVERIFY(converter.result().isEmpty());
        converter.clear();
        QCOMPARE(changed.size(), 8);
        // Another object has independent state and conversion options.
        TestConverter other;
        QCOMPARE(other.state(), QStringLiteral("idle"));
        other.convert(QStringLiteral("ĐặNG"), "none", "keep", false);
        QCOMPARE(other.result(), QStringLiteral("ĐặNG"));
        QCOMPARE(converter.state(), QStringLiteral("idle"));
    }
};

QTEST_GUILESS_MAIN(LinuxTextConverterTests)
#include "LinuxTextConverterTests.moc"
