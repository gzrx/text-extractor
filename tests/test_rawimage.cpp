#include <QTest>
#include <QImage>
#include "capture/rawimage.h"

class TestRawImage : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void decodesA2x2Image()
    {
        // 2x2 ARGB32 image, 4 bytes per pixel, stride 8.
        QByteArray bytes(16, '\0');
        bytes[0] = char(0xFF); // blue channel of pixel (0,0) in little-endian ARGB32
        bytes[3] = char(0xFF); // alpha channel of pixel (0,0)

        const QImage image = textract::imageFromRaw(
            bytes, 2, 2, 8, quint32(QImage::Format_ARGB32));

        QVERIFY(!image.isNull());
        QCOMPARE(image.width(), 2);
        QCOMPARE(image.height(), 2);
        QCOMPARE(image.format(), QImage::Format_ARGB32);
        QCOMPARE(image.pixelColor(0, 0), QColor(0, 0, 255));
    }

    void rejectsTruncatedBuffer()
    {
        const QByteArray bytes(8, '\0'); // needs 16 for 2x2 at stride 8
        QVERIFY(textract::imageFromRaw(bytes, 2, 2, 8,
                                       quint32(QImage::Format_ARGB32)).isNull());
    }

    void rejectsInvalidFormat()
    {
        const QByteArray bytes(16, '\0');
        QVERIFY(textract::imageFromRaw(bytes, 2, 2, 8, 9999).isNull());
    }

    void rejectsZeroDimensions()
    {
        const QByteArray bytes(16, '\0');
        QVERIFY(textract::imageFromRaw(bytes, 0, 2, 8,
                                       quint32(QImage::Format_ARGB32)).isNull());
    }
};

QTEST_MAIN(TestRawImage)
#include "test_rawimage.moc"
