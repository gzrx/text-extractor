// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace textract {

/**
 * Downloads every manifest file missing or mismatched in `dir`.
 *
 * Blocking, for use from --fetch-models. Per-file progress goes to stdout.
 * Returns true only if all four manifest files are present and verified when
 * it returns; on false, `*error` says what went wrong.
 *
 * The invariant, and everything else follows from it:
 *
 *     `dir` only ever contains complete, verified files.
 *
 * Bytes are hashed in memory and the file is written only after the SHA-256
 * matches, so an interrupted or corrupt fetch cannot leave a half-file that
 * makes OnnxPaddleEngine::available() true -- which would produce exactly the
 * confident-nonsense failure mode the charset guard exists to prevent
 * (HANDOFF 13.3). The largest file is ~21 MB, so buffering it is cheap.
 *
 * Idempotent, and therefore also a repair tool: files already present and
 * matching are skipped, and a present file whose checksum differs is
 * re-fetched. That is what makes it a sensible thing for the daemon's
 * "models failed to load" notification to point at.
 */
bool fetchModels(const QString &dir, QString *error);

} // namespace textract
