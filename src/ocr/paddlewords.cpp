// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ocr/paddlewords.h"

namespace textract {

std::vector<Word> wordsFromLines(const std::vector<TextLine> &lines)
{
    std::vector<Word> words;

    for (size_t index = 0; index < lines.size(); ++index) {
        const TextLine &line = lines[index];
        if (line.text.trimmed().isEmpty()) {
            continue;
        }

        // Split on spaces but keep track of where each token started, so the
        // gaps between words are charged to the geometry rather than closed up.
        const qsizetype characters = line.text.size();
        const double cell = characters > 0
                                ? double(line.bbox.width()) / double(characters)
                                : 0.0;

        qsizetype cursor = 0;
        while (cursor < characters) {
            while (cursor < characters && line.text.at(cursor).isSpace()) {
                ++cursor;
            }
            const qsizetype start = cursor;
            while (cursor < characters && !line.text.at(cursor).isSpace()) {
                ++cursor;
            }
            if (cursor == start) {
                break;
            }

            Word word;
            word.text = line.text.mid(start, cursor - start);
            word.confidence = line.confidence;
            word.line = int(index);
            word.block = 0;

            const int left = line.bbox.left() + int(double(start) * cell);
            const int width = int(double(cursor - start) * cell);
            word.bbox = QRect(left, line.bbox.top(), width, line.bbox.height());

            words.push_back(std::move(word));
        }
    }

    return words;
}

} // namespace textract
