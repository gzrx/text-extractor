// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QFont>
#include <QImage>
#include <QPainter>
#include <QRect>
#include <QString>
#include <QStringList>

#include "ocr/word.h"

/// Synthetic crops shared by the engine and pipeline tests. Rendered rather
/// than captured on purpose: a captured image would drag a real desktop's
/// contents into the repository, which is a hazard the fixture corpus already
/// has to manage.
namespace textract::testimages {

/// `text` at a size Tesseract reads reliably, in the given polarity.
inline QImage renderText(const QString &text, Qt::GlobalColor background,
                         Qt::GlobalColor foreground)
{
    QImage image(900, 160, QImage::Format_RGB32);
    image.fill(background);

    QPainter painter(&image);
    QFont font(QStringLiteral("DejaVu Sans"), 48);
    painter.setFont(font);
    painter.setPen(foreground);
    painter.drawText(QRect(20, 20, 860, 120), Qt::AlignLeft | Qt::AlignVCenter,
                     text);
    return image;
}

/// Twelve lines of plausible column text, tagged so reading order is
/// unambiguous in an assertion.
inline QStringList columnLines(const QString &tag)
{
    QStringList lines;
    for (int i = 1; i <= 12; ++i) {
        lines << QStringLiteral("%1 line number %2 of the column")
                     .arg(tag).arg(i);
    }
    return lines;
}

/// Two text columns separated by a wide blank gutter — the layout that makes
/// page segmentation observable, because the two modes read it in different
/// orders. The gutter is roughly a third of a column wide and there are enough
/// lines either side for Tesseract to be confident: its column detector wants
/// evidence, and a handful of short lines does not supply it.
inline QImage renderTwoColumns(const QStringList &left, const QStringList &right)
{
    QImage image(1400, 900, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    QFont font(QStringLiteral("DejaVu Serif"), 28);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(QRect(60, 40, 540, 820), Qt::AlignLeft | Qt::AlignTop,
                     left.join(QLatin1Char('\n')));
    painter.drawText(QRect(800, 40, 540, 820), Qt::AlignLeft | Qt::AlignTop,
                     right.join(QLatin1Char('\n')));
    return image;
}

/// Index of the first word containing `tag`, or -1. Asserting on tag positions
/// rather than on a whole joined string keeps a test about reading order and
/// not about whether Tesseract misread one glyph out of a hundred.
inline int firstIndexOf(const std::vector<Word> &words, const QString &tag)
{
    for (size_t i = 0; i < words.size(); ++i) {
        if (words[i].text.contains(tag)) {
            return int(i);
        }
    }
    return -1;
}

inline int lastIndexOf(const std::vector<Word> &words, const QString &tag)
{
    for (int i = int(words.size()) - 1; i >= 0; --i) {
        if (words[size_t(i)].text.contains(tag)) {
            return i;
        }
    }
    return -1;
}

} // namespace textract::testimages
