// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QTest>
#include "capture/kwincapture.h"

class TestCaptureError : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void namesTheBinaryInBothCases()
    {
        const QString path = QStringLiteral("/opt/textract/bin/textract");
        QVERIFY(textract::authorisationErrorText(path, false).contains(path));
        QVERIFY(textract::authorisationErrorText(path, true).contains(path));
    }

    void asksForTheDesktopFileWhenTheBinaryIsIntact()
    {
        const QString text = textract::authorisationErrorText(
            QStringLiteral("/opt/textract/bin/textract"), false);

        QVERIFY(text.contains(
            QStringLiteral("X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2")));
        QVERIFY(text.contains(QStringLiteral(".desktop")));
    }

    /**
     * The case that motivated splitting these apart: rebuilding relinks the
     * binary under a running daemon, and the desktop-file remedy is then a
     * dead end because nothing about the desktop file is wrong.
     */
    void asksForARestartWhenTheBinaryWasReplaced()
    {
        const QString text = textract::authorisationErrorText(
            QStringLiteral("/opt/textract/bin/textract"), true);

        QVERIFY(text.contains(QStringLiteral("restart"), Qt::CaseInsensitive));
        QVERIFY(text.contains(QStringLiteral("replaced")));
    }

    /// Recommending the desktop-file fix here is what sent the last debugging
    /// session down the wrong path, so assert it is absent rather than merely
    /// de-emphasised.
    void doesNotRecommendTheDesktopFileFixWhenTheBinaryWasReplaced()
    {
        const QString text = textract::authorisationErrorText(
            QStringLiteral("/opt/textract/bin/textract"), true);

        QVERIFY(!text.contains(QStringLiteral("cp build/")));
        QVERIFY(!text.contains(QStringLiteral("kbuildsycoca6")));
    }

    /// Reads /proc/self/exe. The test binary itself is intact on disk, so this
    /// must be false; it mainly guards against the probe reporting true for
    /// every process, which would invert the message.
    void doesNotReportAReplacementForAnIntactBinary()
    {
        QCOMPARE(textract::executableWasReplaced(), false);
    }
};

QTEST_MAIN(TestCaptureError)
#include "test_captureerror.moc"
