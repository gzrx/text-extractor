// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "models/modelspec.h"

#include <QCryptographicHash>
#include <QFile>

namespace textract {

namespace {

// Pinned from files downloaded fresh from the official repos on 2026-08-03,
// NOT from any copy already on this machine. A hash taken from an unverified
// local file is a hash that always passes and assures nothing.
//
// The four hand-installed copies in ~/.local/share/textract/models were then
// compared against these and matched byte for byte, which also confirms M6a's
// corpus scores were measured against exactly these weights.
constexpr char kDetOnnxSha256[] =
    "d73e0058b7a8086bbd57f3d10b8bcd4ff95363f67e06e2762b5e814fe9c9410e";
constexpr char kDetYmlSha256[] =
    "193f435274bf9f0b5f71a929bbfbcf148282df7e633b34e7c373e8f44741b516";
constexpr char kRecOnnxSha256[] =
    "5435fd747c9e0efe15a96d0b378d5bd157e9492ed8fd80edf08f30d02fa24634";
constexpr char kRecYmlSha256[] =
    "ab078671bb49f06228eadccd34f1bb501e157f7a047095ffb943ba81512c77d1";

} // namespace

const std::vector<ModelFile> &modelManifest()
{
    // Official PaddlePaddle exports, Apache-2.0, compatible with this
    // project's GPL-3.0. Prefer these over any third-party conversion.
    static const std::vector<ModelFile> files = {
        {QStringLiteral("PaddlePaddle/PP-OCRv6_small_det_onnx"),
         QStringLiteral("inference.onnx"),
         QStringLiteral("ppocrv6_small_det.onnx"),
         QString::fromLatin1(kDetOnnxSha256)},
        {QStringLiteral("PaddlePaddle/PP-OCRv6_small_det_onnx"),
         QStringLiteral("inference.yml"),
         QStringLiteral("ppocrv6_small_det.yml"),
         QString::fromLatin1(kDetYmlSha256)},
        {QStringLiteral("PaddlePaddle/PP-OCRv6_small_rec_onnx"),
         QStringLiteral("inference.onnx"),
         QStringLiteral("ppocrv6_small_rec.onnx"),
         QString::fromLatin1(kRecOnnxSha256)},
        {QStringLiteral("PaddlePaddle/PP-OCRv6_small_rec_onnx"),
         QStringLiteral("inference.yml"),
         QStringLiteral("ppocrv6_small_rec.yml"),
         QString::fromLatin1(kRecYmlSha256)},
    };
    return files;
}

QString modelUrl(const ModelFile &file)
{
    return QStringLiteral("https://huggingface.co/%1/resolve/main/%2")
        .arg(file.remoteRepo, file.remoteName);
}

QStringList modelsMissing(const QString &dir)
{
    QStringList missing;
    for (const ModelFile &file : modelManifest()) {
        if (!QFile::exists(dir + QLatin1Char('/') + file.localName)) {
            missing << file.localName;
        }
    }
    return missing;
}

bool modelFileValid(const QString &dir, const ModelFile &file)
{
    QFile in(dir + QLatin1Char('/') + file.localName);
    if (!in.open(QIODevice::ReadOnly)) {
        return false;
    }
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&in)) {
        return false;
    }
    return QString::fromLatin1(hash.result().toHex()) == file.sha256;
}

QString sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(
        QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
}

bool verifyChecksum(const ModelFile &file, const QByteArray &data)
{
    return sha256Hex(data) == file.sha256;
}

} // namespace textract
