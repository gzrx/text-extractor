// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include "app/tier2cache.h"

using namespace textract;

namespace {

QImage solid(int w, int h, QRgb colour)
{
    QImage image(w, h, QImage::Format_ARGB32);
    image.fill(colour);
    return image;
}

} // namespace

class TestTier2Cache : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void emptyCacheHasNoCrop();
    void storeRoundTripsPixelsAndForcedKind();
    void storeRoundTripsAnUnsetForcedKind();
    void cropIsCachedEvenWhenNoTextWasRecognised();
    void changesTextComparesAgainstTheStoredText();
    void storingAgainReplacesTheEntry();
};

void TestTier2Cache::emptyCacheHasNoCrop()
{
    Tier2Cache cache;
    QVERIFY(!cache.hasCrop());
}

void TestTier2Cache::storeRoundTripsPixelsAndForcedKind()
{
    Tier2Cache cache;
    const QImage crop = solid(4, 3, qRgb(10, 20, 30));
    cache.store(crop, LayoutKind::Raw, QStringLiteral("hello"));

    QVERIFY(cache.hasCrop());
    QCOMPARE(cache.crop(), crop);
    QCOMPARE(cache.forcedKind(), std::optional<LayoutKind>(LayoutKind::Raw));
    QCOMPARE(cache.previousText(), QStringLiteral("hello"));
}

/// The fresh-capture tier-2 path stores nullopt on purpose: Shift was consumed
/// by the Shift+Calculator shortcut and must not also be read as force-Raw.
void TestTier2Cache::storeRoundTripsAnUnsetForcedKind()
{
    Tier2Cache cache;
    cache.store(solid(2, 2, qRgb(0, 0, 0)), std::nullopt, QStringLiteral("x"));
    QVERIFY(cache.hasCrop());
    QVERIFY(!cache.forcedKind().has_value());
}

/// Deliberate, and it reads like an oversight without this test. Tier 2
/// recognising text where tier 1 recognised none is the escalation working --
/// that is the moment the user most wants the second key, so the crop must
/// survive an empty tier-1 result.
void TestTier2Cache::cropIsCachedEvenWhenNoTextWasRecognised()
{
    Tier2Cache cache;
    cache.store(solid(5, 5, qRgb(1, 2, 3)), std::nullopt, QString());

    QVERIFY(cache.hasCrop());
    QVERIFY(cache.previousText().isEmpty());
    QVERIFY(cache.changesText(QStringLiteral("found something")));
}

void TestTier2Cache::changesTextComparesAgainstTheStoredText()
{
    Tier2Cache cache;
    cache.store(solid(2, 2, qRgb(0, 0, 0)), std::nullopt, QStringLiteral("same"));

    QVERIFY(!cache.changesText(QStringLiteral("same")));
    QVERIFY(cache.changesText(QStringLiteral("different")));
}

/// The stored text is the PREVIOUS text, not specifically tier 1's. A second
/// escalation therefore compares tier 2 against tier 2, which is the right
/// question: "did this press change my clipboard".
void TestTier2Cache::storingAgainReplacesTheEntry()
{
    Tier2Cache cache;
    const QImage first = solid(2, 2, qRgb(255, 0, 0));
    const QImage second = solid(3, 3, qRgb(0, 255, 0));

    cache.store(first, LayoutKind::Raw, QStringLiteral("one"));
    cache.store(second, LayoutKind::Prose, QStringLiteral("two"));

    QCOMPARE(cache.crop(), second);
    QCOMPARE(cache.forcedKind(), std::optional<LayoutKind>(LayoutKind::Prose));
    QCOMPARE(cache.previousText(), QStringLiteral("two"));
    QVERIFY(!cache.changesText(QStringLiteral("two")));
    QVERIFY(cache.changesText(QStringLiteral("one")));
}

QTEST_GUILESS_MAIN(TestTier2Cache)
#include "test_tier2cache.moc"
