// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "assemble/assemble.h"

namespace textract {

namespace {

QString assembleRaw(const std::vector<Word> &words)
{
    QString out;
    int previousLine = -1;
    int previousBlock = -1;

    for (const Word &word : words) {
        if (previousLine >= 0) {
            if (word.block != previousBlock) {
                out += QStringLiteral("\n\n");
            } else if (word.line != previousLine) {
                out += QLatin1Char('\n');
            } else {
                out += QLatin1Char(' ');
            }
        }
        out += word.text;
        previousLine = word.line;
        previousBlock = word.block;
    }

    return out;
}

} // namespace

QString assemble(const std::vector<Word> &words, LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::Raw:
    case LayoutKind::Code:
    case LayoutKind::Prose:
    case LayoutKind::Table:
        // M2 implements Raw. M4 replaces the other three branches.
        return assembleRaw(words);
    }
    return QString();
}

} // namespace textract
