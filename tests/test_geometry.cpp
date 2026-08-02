// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include "overlay/geometry.h"

class TestGeometry : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void scalesByDevicePixelRatio()
    {
        // A 100x50 logical selection at logical (10,20) on a 1.25-scale screen
        // whose origin is the desktop origin.
        const QRect physical = textract::mapSelectionToPhysical(
            QRect(10, 20, 100, 50), QPoint(0, 0), 1.25, QSize(2560, 1600));

        QCOMPARE(physical, QRect(13, 25, 125, 63));
    }

    void offsetsBySecondScreenOrigin()
    {
        // Same selection, but the overlay is on a screen starting at logical
        // (2048, 0) — i.e. physical (2560, 0) at 1.25 scale.
        const QRect physical = textract::mapSelectionToPhysical(
            QRect(10, 20, 100, 50), QPoint(2048, 0), 1.25, QSize(5120, 1600));

        QCOMPARE(physical.x(), 2573); // (2048 + 10) * 1.25 = 2572.5 -> 2573
        QCOMPARE(physical.y(), 25);
        QCOMPARE(physical.width(), 125);
    }

    void clampsToImageBounds()
    {
        // (2000,1250) 200x200 logical -> (2500,1563) 250x250 physical, which
        // overhangs a 2560x1600 image on both axes and must be truncated.
        const QRect physical = textract::mapSelectionToPhysical(
            QRect(2000, 1250, 200, 200), QPoint(0, 0), 1.25, QSize(2560, 1600));

        // Asserted as exact values: a bounds-only check passes vacuously for
        // an empty rect and would not catch a mapping that returns nothing.
        QCOMPARE(physical, QRect(2500, 1563, 60, 37));
        QVERIFY(!physical.isEmpty());
    }

    void returnsEmptyForDegenerateInput()
    {
        const QSize image(2560, 1600);
        QVERIFY(textract::mapSelectionToPhysical(QRect(), QPoint(0, 0), 1.25, image).isEmpty());
        QVERIFY(textract::mapSelectionToPhysical(QRect(0, 0, 10, 10), QPoint(0, 0), 0.0, image).isEmpty());
        QVERIFY(textract::mapSelectionToPhysical(QRect(0, 0, 10, 10), QPoint(0, 0), -1.0, image).isEmpty());
    }

    void selectionFullyOutsideImageIsUnusable()
    {
        const QRect physical = textract::mapSelectionToPhysical(
            QRect(9000, 9000, 100, 100), QPoint(0, 0), 1.25, QSize(2560, 1600));

        QVERIFY(physical.isEmpty());
        QVERIFY(!textract::isSelectionUsable(physical));
    }

    void identityAtScaleOne()
    {
        const QRect physical = textract::mapSelectionToPhysical(
            QRect(10, 20, 100, 50), QPoint(0, 0), 1.0, QSize(1920, 1080));

        QCOMPARE(physical, QRect(10, 20, 100, 50));
    }

    void rejectsTinySelections()
    {
        QVERIFY(!textract::isSelectionUsable(QRect(0, 0, 4, 100)));
        QVERIFY(!textract::isSelectionUsable(QRect(0, 0, 100, 4)));
        QVERIFY(!textract::isSelectionUsable(QRect()));
        QVERIFY(textract::isSelectionUsable(QRect(0, 0, 40, 20)));
    }
};

QTEST_MAIN(TestGeometry)
#include "test_geometry.moc"
