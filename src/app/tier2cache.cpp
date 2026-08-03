// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/tier2cache.h"

namespace textract {

void Tier2Cache::store(const QImage &crop,
                       std::optional<LayoutKind> forcedKind,
                       const QString &text)
{
    m_crop = crop;
    m_forcedKind = forcedKind;
    m_previousText = text;
}

} // namespace textract
