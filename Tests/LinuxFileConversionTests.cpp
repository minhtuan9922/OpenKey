#include "FileConversion.h"
#include <QFile>
#include <QDir>
#include <QTemporaryDir>
#include <QtTest>
class FileTests : public QObject {
    Q_OBJECT
    QTemporaryDir directory;
    QString write(const QString& name, const QByteArray& bytes) {
        const auto path = directory.filePath(name); QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size()) return {};
        return path;
    }
    QByteArray read(const QString& name) { QFile file(name); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll(); }
    QUrl url(const QString& path) { return QUrl::fromLocalFile(path); }
    QString stub(const QString& code) {
        const auto path = write("worker.py", QByteArray("#!/usr/bin/python3\n") + code.toUtf8());
        QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner); return path;
    }
private slots:
    void largeUnicode() {
        const QString text = QString("Tiếng Việt 😀\n").repeated(15000);
        const auto input = write("tiếng Việt.txt", text.toUtf8()), output = directory.filePath("kết quả.txt");
        QVERIFY(!input.isEmpty());
        FileConversion converter(OPENKEY_FILE_WORKER_BINARY);
        converter.start(url(input), url(output), "unicode", "compound");
        QTRY_COMPARE_WITH_TIMEOUT(converter.state(), QString("converted"), 10000);
        QCOMPARE(QString::fromUtf8(read(output)), text.normalized(QString::NormalizationForm_D));
        QCOMPARE(read(input), text.toUtf8());
    }
    void invalidAndLimits() {
        FileConversion converter(OPENKEY_FILE_WORKER_BINARY);
        const auto input = write("invalid.txt", QByteArray::fromHex("ff"));
        const auto output = write("keep.txt", "KEEP");
        converter.start(url(input), url(output), "unicode", "compound");
        QTRY_COMPARE(converter.state(), QString("failed")); QCOMPARE(read(output), "KEEP");
        converter.start(url(output), url(output), "unicode", "unicode"); QCOMPARE(converter.state(), "invalid");
        converter.start(QUrl("https://example.com/file"), url(output), "unicode", "unicode"); QCOMPARE(converter.state(), "invalid");
        const auto huge = write("huge.txt", QByteArray(16 * 1024 * 1024 + 1, 'a'));
        converter.start(url(huge), url(output), "unicode", "unicode"); QCOMPARE(converter.state(), "invalid-input");
        QVERIFY(QFile::link(input, directory.filePath("link.txt")));
        converter.start(url(directory.filePath("link.txt")), url(output), "unicode", "unicode"); QCOMPARE(converter.state(), "invalid-input");
        QCOMPARE(read(output), "KEEP");
    }
    void cancelTimeoutAndConflict() {
        const auto input = write("source.txt", "source"), output = write("target.txt", "KEEP");
        auto program = stub("import time\ntime.sleep(3)\n");
        {
            FileConversion converter(program);
            converter.start(url(input), url(output), "unicode", "unicode"); converter.cancel();
            QTRY_COMPARE(converter.state(), QString("cancelled")); QCOMPARE(read(output), "KEEP");
        }
        {
            FileConversion converter(program, nullptr, 30);
            converter.start(url(input), url(output), "unicode", "unicode");
            QTRY_COMPARE(converter.state(), QString("timeout")); QCOMPARE(read(output), "KEEP");
        }
        program = stub("import pathlib,sys,time\npathlib.Path(sys.argv[3]).write_text('new')\ntime.sleep(0.2)\nprint('{\"ok\":true}')\n");
        {
            FileConversion converter(program);
            converter.start(url(input), url(output), "unicode", "unicode");
            write("target.txt", "CHANGED");
            QTRY_COMPARE(converter.state(), QString("conflict")); QCOMPARE(read(output), "CHANGED");
        }
        QVERIFY(QDir(directory.path()).entryList({".openkey-convert-*"}, QDir::Files | QDir::Hidden).isEmpty());
    }
    void emptyAndMissingWorker() {
        const auto input = write("empty.txt", ""), output = directory.filePath("empty-result.txt");
        FileConversion converter(OPENKEY_FILE_WORKER_BINARY);
        converter.start(url(input), url(output), "unicode", "unicode"); QTRY_COMPARE(converter.state(), QString("converted"));
        QVERIFY(QFile::exists(output)); QVERIFY(read(output).isEmpty());
        FileConversion missing(directory.filePath("missing-program"));
        missing.start(url(input), url(output), "unicode", "unicode"); QTRY_COMPARE(missing.state(), QString("failed"));
    }
};
QTEST_GUILESS_MAIN(FileTests)
#include "LinuxFileConversionTests.moc"
