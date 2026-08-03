// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QFile>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTest>

#include "models/modelspec.h"

using namespace textract;

class TestModelSpec : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void manifestHasFourFilesIncludingBothYml();
    void everyChecksumIsLowercaseHex();
    void urlsPointAtTheOfficialRepos();
    void sha256MatchesKnownVectors();
    void verifyChecksumAcceptsOnlyTheRightBytes();
    void missingListsEverythingInAnEmptyDirectory();
    void missingIsEmptyWhenAllFourArePresent();
    void missingNamesOnlyTheAbsentFile();
    void modelFileValidRejectsWrongContent();
};

/// The .yml files are not optional metadata: the DB thresholds and the entire
/// 18708-character recognition charset are read out of them (HANDOFF 13.3). A
/// fetch that got only the .onnx files would produce an engine that cannot
/// start, or one running against stale thresholds.
void TestModelSpec::manifestHasFourFilesIncludingBothYml()
{
    const auto &files = modelManifest();
    QCOMPARE(files.size(), size_t(4));

    QStringList names;
    for (const ModelFile &f : files) {
        names << f.localName;
    }
    names.sort();
    QCOMPARE(names, (QStringList{QStringLiteral("ppocrv6_small_det.onnx"),
                                 QStringLiteral("ppocrv6_small_det.yml"),
                                 QStringLiteral("ppocrv6_small_rec.onnx"),
                                 QStringLiteral("ppocrv6_small_rec.yml")}));
}

void TestModelSpec::everyChecksumIsLowercaseHex()
{
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
    for (const ModelFile &f : modelManifest()) {
        QVERIFY2(hex.match(f.sha256).hasMatch(),
                 qPrintable(f.localName + QStringLiteral(": ") + f.sha256));
    }
}

void TestModelSpec::urlsPointAtTheOfficialRepos()
{
    for (const ModelFile &f : modelManifest()) {
        const QString url = modelUrl(f);
        QVERIFY2(url.startsWith(QStringLiteral(
                     "https://huggingface.co/PaddlePaddle/PP-OCRv6_small_")),
                 qPrintable(url));
        QVERIFY2(url.endsWith(QStringLiteral("/resolve/main/") + f.remoteName),
                 qPrintable(url));
    }
}

void TestModelSpec::sha256MatchesKnownVectors()
{
    QCOMPARE(sha256Hex(QByteArray()),
             QStringLiteral("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934c"
                            "a495991b7852b855"));
    QCOMPARE(sha256Hex(QByteArrayLiteral("abc")),
             QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9c"
                            "b410ff61f20015ad"));
}

void TestModelSpec::verifyChecksumAcceptsOnlyTheRightBytes()
{
    ModelFile f;
    f.sha256 = sha256Hex(QByteArrayLiteral("abc"));
    QVERIFY(verifyChecksum(f, QByteArrayLiteral("abc")));
    QVERIFY(!verifyChecksum(f, QByteArrayLiteral("abd")));
    QVERIFY(!verifyChecksum(f, QByteArray()));
}

void TestModelSpec::missingListsEverythingInAnEmptyDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QCOMPARE(modelsMissing(dir.path()).size(), 4);
}

void TestModelSpec::missingIsEmptyWhenAllFourArePresent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const ModelFile &f : modelManifest()) {
        QFile out(dir.path() + QLatin1Char('/') + f.localName);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("x");
    }
    // Existence only -- modelsMissing() deliberately does not hash. Content is
    // modelFileValid()'s job, so the daemon's "is it installed" check stays
    // cheap and does not read 31 MB on every keypress.
    QVERIFY(modelsMissing(dir.path()).isEmpty());
}

void TestModelSpec::missingNamesOnlyTheAbsentFile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (const ModelFile &f : modelManifest()) {
        if (f.localName == QLatin1String("ppocrv6_small_rec.yml")) {
            continue;
        }
        QFile out(dir.path() + QLatin1Char('/') + f.localName);
        QVERIFY(out.open(QIODevice::WriteOnly));
        out.write("x");
    }
    QCOMPARE(modelsMissing(dir.path()),
             QStringList{QStringLiteral("ppocrv6_small_rec.yml")});
}

void TestModelSpec::modelFileValidRejectsWrongContent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const ModelFile &f = modelManifest().front();

    QVERIFY2(!modelFileValid(dir.path(), f), "absent file must not be valid");

    QFile out(dir.path() + QLatin1Char('/') + f.localName);
    QVERIFY(out.open(QIODevice::WriteOnly));
    out.write("not the model");
    out.close();
    QVERIFY2(!modelFileValid(dir.path(), f), "wrong content must not be valid");
}

QTEST_GUILESS_MAIN(TestModelSpec)
#include "test_modelspec.moc"
