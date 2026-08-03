// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "correct/dictionary.h"

#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>

#include <hunspell.hxx>

namespace textract {

namespace {

/**
 * Where distributions put Hunspell langdata.
 *
 * Hunspell itself has no search path — it takes two absolute filenames — so
 * this list is the whole of the lookup. The user location comes first so that
 * a hand-installed dictionary wins over the packaged one.
 */
QStringList dictionarySearchPaths()
{
    QStringList paths = QStandardPaths::standardLocations(
        QStandardPaths::GenericDataLocation);
    for (QString &path : paths) {
        path += QStringLiteral("/hunspell");
    }
    paths << QStringLiteral("/usr/share/hunspell")
          << QStringLiteral("/usr/share/myspell")
          << QStringLiteral("/usr/share/myspell/dicts");
    return paths;
}

} // namespace

Dictionary::Dictionary(const QString &language)
{
    for (const QString &directory : dictionarySearchPaths()) {
        const QString base = directory + QLatin1Char('/') + language;
        const QString affix = base + QStringLiteral(".aff");
        const QString words = base + QStringLiteral(".dic");

        // Both halves or neither: Hunspell asserts nothing about the files it
        // is handed and an .aff without its .dic loads as an empty dictionary,
        // which would report every word as misspelled.
        if (QFileInfo::exists(affix) && QFileInfo::exists(words)) {
            m_hunspell = std::make_unique<Hunspell>(
                affix.toLocal8Bit().constData(),
                words.toLocal8Bit().constData());
            m_utf8 = qstrcmp(m_hunspell->get_dic_encoding(), "UTF-8") == 0;
            return;
        }
    }
}

Dictionary::~Dictionary() = default;

bool Dictionary::contains(const QString &word) const
{
    if (!m_hunspell || word.isEmpty()) {
        return false;
    }
    // Hunspell wants the word in the encoding its own .aff declared, so the
    // SET line read at load time decides this rather than an assumption.
    const QByteArray encoded = m_utf8 ? word.toUtf8() : word.toLocal8Bit();
    return m_hunspell->spell(
        std::string(encoded.constData(), size_t(encoded.size())));
}

} // namespace textract
