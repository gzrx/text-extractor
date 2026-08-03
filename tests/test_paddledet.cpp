// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>

#include <vector>

#include "ocr/paddledet.h"

namespace {

/// A probability map with `value` inside `regions` and 0 everywhere else.
std::vector<float> mapWith(int width, int height,
                           const std::vector<QRect> &regions, float value)
{
    std::vector<float> map(size_t(width * height), 0.0f);
    for (const QRect &region : regions) {
        for (int y = region.top(); y <= region.bottom(); ++y) {
            for (int x = region.left(); x <= region.right(); ++x) {
                map[size_t(y * width + x)] = value;
            }
        }
    }
    return map;
}

} // namespace

class TestPaddleDet : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void findsASingleBlob()
    {
        const auto map = mapWith(100, 50, {QRect(10, 10, 40, 12)}, 0.9f);

        const auto boxes = textract::detectTextLines(map.data(), 100, 50, {});

        QCOMPARE(boxes.size(), size_t(1));
        // Dilated outward, so it contains the source region.
        QVERIFY(boxes[0].contains(QRect(10, 10, 40, 12)));
    }

    void findsTwoSeparatedBlobs()
    {
        const auto map = mapWith(100, 60,
                                 {QRect(5, 5, 30, 10), QRect(5, 40, 30, 10)},
                                 0.9f);

        const auto boxes = textract::detectTextLines(map.data(), 100, 60, {});

        QCOMPARE(boxes.size(), size_t(2));
    }

    /// Everything under `thresh` is background, however large.
    void ignoresProbabilitiesBelowTheThreshold()
    {
        const auto map = mapWith(100, 50, {QRect(10, 10, 40, 12)}, 0.1f);

        textract::DetectOptions options;
        options.thresh = 0.2f;

        const auto boxes = textract::detectTextLines(map.data(), 100, 50, options);

        QVERIFY(boxes.empty());
    }

    /// A blob whose mean probability is weak is a smear, not a text line.
    void rejectsABlobBelowTheBoxThreshold()
    {
        const auto map = mapWith(100, 50, {QRect(10, 10, 40, 12)}, 0.25f);

        textract::DetectOptions options;
        options.thresh = 0.2f;
        options.boxThresh = 0.45f;

        const auto boxes = textract::detectTextLines(map.data(), 100, 50, options);

        QVERIFY(boxes.empty());
    }

    /// An antialiasing sliver is not a line.
    void rejectsABlobThinnerThanMinSide()
    {
        const auto map = mapWith(100, 50, {QRect(10, 10, 40, 1)}, 0.9f);

        textract::DetectOptions options;
        options.minSide = 3;

        const auto boxes = textract::detectTextLines(map.data(), 100, 50, options);

        QVERIFY(boxes.empty());
    }

    /// The dilation is a uniform offset, NOT a fraction of the box's own width.
    ///
    /// A long text line grown by a proportion of its width gains tens of pixels
    /// horizontally and bridges the gutter between two columns of prose, which
    /// destroys the only geometric signal that a column boundary exists.
    /// Measured: pdf-two-column found zero column gaps and scored 0.3310 that
    /// way. Guard the property directly -- horizontal growth must stay
    /// comparable to vertical growth however wide the line is.
    void dilatesUniformlyRatherThanByAFractionOfWidth()
    {
        const auto map = mapWith(400, 60, {QRect(20, 20, 240, 16)}, 0.9f);

        const auto boxes = textract::detectTextLines(map.data(), 400, 60, {});

        QCOMPARE(boxes.size(), size_t(1));
        const int grownLeft = 20 - boxes[0].left();
        const int grownTop = 20 - boxes[0].top();
        QCOMPARE(grownLeft, grownTop);
        // 240x16: area 3840, perimeter 512, offset = 3840 * 1.4 / 512 = 10.
        QCOMPARE(grownLeft, 10);
    }

    void returnsNothingForAnEmptyMap()
    {
        const std::vector<float> map(size_t(100 * 50), 0.0f);

        QVERIFY(textract::detectTextLines(map.data(), 100, 50, {}).empty());
    }

    void survivesDegenerateDimensions()
    {
        const std::vector<float> map(1, 0.9f);

        QVERIFY(textract::detectTextLines(map.data(), 0, 0, {}).empty());
        QVERIFY(textract::detectTextLines(nullptr, 10, 10, {}).empty());
    }

    /// Boxes are clamped to the map: a blob at the edge must not dilate out of
    /// bounds and produce coordinates the caller will index with.
    void clampsDilationToTheMapBounds()
    {
        const auto map = mapWith(50, 30, {QRect(0, 0, 20, 10)}, 0.9f);

        const auto boxes = textract::detectTextLines(map.data(), 50, 30, {});

        QCOMPARE(boxes.size(), size_t(1));
        QVERIFY(boxes[0].left() >= 0);
        QVERIFY(boxes[0].top() >= 0);
        QVERIFY(boxes[0].right() < 50);
        QVERIFY(boxes[0].bottom() < 30);
    }
};

QTEST_MAIN(TestPaddleDet)
#include "test_paddledet.moc"
