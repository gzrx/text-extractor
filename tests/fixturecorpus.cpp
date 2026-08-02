// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fixturecorpus.h"

#include <algorithm>
#include <vector>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace textract::fixtures {

namespace {

bool parseLayout(const QString &name, LayoutKind *out)
{
    const QString key = name.toLower();
    if (key == QLatin1String("raw"))   { *out = LayoutKind::Raw;   return true; }
    if (key == QLatin1String("code"))  { *out = LayoutKind::Code;  return true; }
    if (key == QLatin1String("prose")) { *out = LayoutKind::Prose; return true; }
    if (key == QLatin1String("table")) { *out = LayoutKind::Table; return true; }
    return false;
}

/// Levenshtein distance, two rows rather than the full matrix.
int editDistance(const QString &a, const QString &b)
{
    const int n = a.size();
    const int m = b.size();
    if (n == 0) {
        return m;
    }
    if (m == 0) {
        return n;
    }

    std::vector<int> previous(m + 1);
    std::vector<int> current(m + 1);
    for (int j = 0; j <= m; ++j) {
        previous[j] = j;
    }

    for (int i = 1; i <= n; ++i) {
        current[0] = i;
        for (int j = 1; j <= m; ++j) {
            const int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            current[j] = std::min({current[j - 1] + 1,
                                   previous[j] + 1,
                                   previous[j - 1] + cost});
        }
        previous.swap(current);
    }
    return previous[m];
}

/// Directories that may hold traineddata, most specific first.
QStringList tessdataCandidates()
{
    QStringList dirs;
    if (const QByteArray prefix = qgetenv("TESSDATA_PREFIX"); !prefix.isEmpty()) {
        const QString path = QString::fromLocal8Bit(prefix);
        // TESSDATA_PREFIX has meant both the tessdata directory and its parent
        // across Tesseract versions, so try it both ways.
        dirs << path << path + QStringLiteral("/tessdata");
    }
    dirs << QStringLiteral("/usr/share/tessdata")
         << QStringLiteral("/usr/share/tesseract-ocr/5/tessdata")
         << QStringLiteral("/usr/share/tesseract-ocr/tessdata")
         << QStringLiteral("/usr/local/share/tessdata");
    return dirs;
}

} // namespace

std::vector<Fixture> loadManifest(const QString &manifestPath, QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return std::vector<Fixture>();
    };

    if (error) {
        error->clear();
    }

    QFile file(manifestPath);
    if (!file.exists()) {
        // No corpus captured yet. Not an error.
        return {};
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("cannot read %1").arg(manifestPath));
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(),
                                                           &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return fail(QStringLiteral("%1: %2 at offset %3")
                        .arg(manifestPath, parseError.errorString())
                        .arg(parseError.offset));
    }
    if (!document.isObject() || !document.object().contains(QLatin1String("fixtures"))) {
        return fail(QStringLiteral("%1: expected an object with a \"fixtures\" array")
                        .arg(manifestPath));
    }

    const QDir base = QFileInfo(manifestPath).absoluteDir();
    const QJsonArray entries = document.object()
                                   .value(QLatin1String("fixtures")).toArray();

    std::vector<Fixture> fixtures;
    fixtures.reserve(size_t(entries.size()));

    for (int i = 0; i < entries.size(); ++i) {
        const QJsonObject entry = entries.at(i).toObject();

        Fixture fixture;
        fixture.name = entry.value(QLatin1String("name")).toString();
        const QString label = fixture.name.isEmpty()
                                  ? QStringLiteral("entry %1").arg(i)
                                  : fixture.name;

        const QString image = entry.value(QLatin1String("image")).toString();
        const QString expected = entry.value(QLatin1String("expected")).toString();
        if (fixture.name.isEmpty() || image.isEmpty() || expected.isEmpty()) {
            return fail(QStringLiteral("%1: %2 needs \"name\", \"image\" and \"expected\"")
                            .arg(manifestPath, label));
        }

        fixture.imagePath = base.absoluteFilePath(image);
        fixture.expectedPath = base.absoluteFilePath(expected);
        fixture.notes = entry.value(QLatin1String("notes")).toString();
        fixture.langs = entry.value(QLatin1String("langs"))
                            .toString(QStringLiteral("eng"));
        fixture.minScore = entry.value(QLatin1String("minScore")).toDouble(0.0);

        const QString layout = entry.value(QLatin1String("layout"))
                                   .toString(QStringLiteral("raw"));
        if (!parseLayout(layout, &fixture.layout)) {
            return fail(QStringLiteral("%1: %2 has unknown layout \"%3\"")
                            .arg(manifestPath, label, layout));
        }

        fixtures.push_back(fixture);
    }

    return fixtures;
}

QString normalise(const QString &text)
{
    QString unified = text;
    unified.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    unified.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList lines = unified.split(QLatin1Char('\n'));
    for (QString &line : lines) {
        while (!line.isEmpty() && line.back().isSpace()) {
            line.chop(1);
        }
    }
    while (!lines.isEmpty() && lines.front().isEmpty()) {
        lines.pop_front();
    }
    while (!lines.isEmpty() && lines.back().isEmpty()) {
        lines.pop_back();
    }

    return lines.join(QLatin1Char('\n'));
}

double similarity(const QString &expected, const QString &actual)
{
    const QString a = normalise(expected);
    const QString b = normalise(actual);

    const int longest = std::max(a.size(), b.size());
    if (longest == 0) {
        return 1.0; // both empty: nothing expected, nothing produced
    }

    return 1.0 - double(editDistance(a, b)) / double(longest);
}

bool langdataAvailable(const QString &langs)
{
    const QStringList components = langs.split(QLatin1Char('+'),
                                               Qt::SkipEmptyParts);
    if (components.isEmpty()) {
        return false;
    }

    const QStringList dirs = tessdataCandidates();
    for (const QString &lang : components) {
        const QString file = lang + QStringLiteral(".traineddata");
        const bool found = std::any_of(dirs.cbegin(), dirs.cend(),
                                       [&file](const QString &dir) {
                                           return QFileInfo::exists(dir + QLatin1Char('/') + file);
                                       });
        if (!found) {
            return false;
        }
    }
    return true;
}

} // namespace textract::fixtures
