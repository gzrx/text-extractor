// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/fetch.h"

#include "models/modelspec.h"

#include <cstdio>

#include <QDir>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>
#include <QUrl>

namespace textract {

namespace {

/// Downloads `url` into memory, reporting progress on stderr.
///
/// stderr rather than stdout so the carriage-returned progress line does not
/// interleave with the per-file result lines, and so redirecting stdout still
/// yields a clean log.
QByteArray download(QNetworkAccessManager &nam, const QString &url,
                    const QString &label, QString *error)
{
    QNetworkRequest request{QUrl(url)};

    // HuggingFace serves the payload from a CDN via a redirect, so following
    // them is not optional here. NoLessSafeRedirectPolicy refuses an
    // https -> http downgrade, which is what keeps that acceptable.
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = nam.get(request);

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::downloadProgress, reply,
                     [&label](qint64 received, qint64 total) {
        if (total > 0) {
            fprintf(stderr, "\r  %-24s %5.1f / %5.1f MB", qPrintable(label),
                    received / 1048576.0, total / 1048576.0);
        } else {
            fprintf(stderr, "\r  %-24s %5.1f MB", qPrintable(label),
                    received / 1048576.0);
        }
        fflush(stderr);
    });
    loop.exec();
    fprintf(stderr, "\r%60s\r", "");
    fflush(stderr);

    const QNetworkReply::NetworkError code = reply->error();
    const QString message = reply->errorString();
    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (code != QNetworkReply::NoError) {
        *error = QStringLiteral("%1: %2 (%3)").arg(label, message, url);
        return QByteArray();
    }
    return data;
}

} // namespace

bool fetchModels(const QString &dir, QString *error)
{
    if (!QDir().mkpath(dir)) {
        *error = QStringLiteral("could not create %1").arg(dir);
        return false;
    }

    QNetworkAccessManager nam;

    for (const ModelFile &file : modelManifest()) {
        // Present and correct: skip. This is what makes re-running cheap, and
        // what makes the command usable as a repair tool.
        if (modelFileValid(dir, file)) {
            printf("  %-24s ok (already installed)\n", qPrintable(file.localName));
            fflush(stdout);
            continue;
        }

        const QByteArray data = download(nam, modelUrl(file), file.localName, error);
        if (!error->isEmpty()) {
            return false;
        }

        // Verify BEFORE anything is written. A blob that does not match is
        // either corruption or tampering, and 31 MB of opaque binary landing
        // in a directory the daemon runs inference from is precisely where
        // that distinction is worth enforcing rather than warning about.
        if (!verifyChecksum(file, data)) {
            *error = QStringLiteral("checksum mismatch for %1\n"
                                    "  expected %2\n"
                                    "  got      %3\n"
                                    "Nothing was written to %4.")
                         .arg(file.localName, file.sha256, sha256Hex(data), dir);
            return false;
        }

        // QSaveFile writes to a temporary in the same directory and renames on
        // commit(), so the rename is atomic rather than a cross-filesystem
        // copy and a crash mid-write leaves no half-file behind.
        QSaveFile out(dir + QLatin1Char('/') + file.localName);
        if (!out.open(QIODevice::WriteOnly)) {
            *error = QStringLiteral("could not open %1 for writing: %2")
                         .arg(file.localName, out.errorString());
            return false;
        }
        if (out.write(data) != data.size() || !out.commit()) {
            *error = QStringLiteral("could not write %1: %2")
                         .arg(file.localName, out.errorString());
            return false;
        }

        printf("  %-24s %6.1f MB  ok sha256\n", qPrintable(file.localName),
               data.size() / 1048576.0);
        fflush(stdout);
    }

    return true;
}

} // namespace textract
