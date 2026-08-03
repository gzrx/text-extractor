// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

#include <QByteArray>
#include <QString>
#include <QStringList>

namespace textract {

/**
 * One model file: where it comes from, what it is called on disk, what it
 * hashes to.
 *
 * The remote and local names differ deliberately. Both HuggingFace repos call
 * their payload `inference.onnx`, so downloading them under their upstream
 * names would collide in a single directory.
 */
struct ModelFile {
    QString remoteRepo; ///< e.g. "PaddlePaddle/PP-OCRv6_small_det_onnx"
    QString remoteName; ///< e.g. "inference.onnx"
    QString localName;  ///< e.g. "ppocrv6_small_det.onnx"
    QString sha256;     ///< lowercase hex, 64 chars
};

/**
 * The four files OnnxPaddleEngine needs.
 *
 * Four, not two. The .yml files are not optional metadata: the DB thresholds
 * and the entire 18708-character recognition charset are read out of them, so
 * an install with only the .onnx files produces an engine that cannot start --
 * or, worse, one running against stale thresholds. See HANDOFF 13.3.
 */
const std::vector<ModelFile> &modelManifest();

/// The download URL for `file`.
QString modelUrl(const ModelFile &file);

/**
 * Local names of the manifest files absent from `dir`, in manifest order.
 *
 * Existence only, deliberately: this runs on the tier-2 keypress path, and
 * hashing 31 MB to answer "is it installed" would be the wrong trade. Content
 * is modelFileValid()'s question, and only --fetch-models asks it.
 */
QStringList modelsMissing(const QString &dir);

/// True if `dir` holds `file` and its bytes hash to `file.sha256`.
bool modelFileValid(const QString &dir, const ModelFile &file);

/// The SHA-256 of `data` as lowercase hex.
QString sha256Hex(const QByteArray &data);

/// True if `data` hashes to `file.sha256`.
bool verifyChecksum(const ModelFile &file, const QByteArray &data);

} // namespace textract
