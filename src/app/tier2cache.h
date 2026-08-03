// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <QImage>
#include <QString>

#include "assemble/assemble.h"

namespace textract {

/**
 * What the tier-2 shortcut re-reads.
 *
 * Three things, and each is load-bearing:
 *
 *   * the exact pixels tier 1 saw -- an escalation that also changed the region
 *     would be comparing two different things, which is the whole reason this
 *     exists rather than asking the user to drag again;
 *   * the layout the user demanded for them, so Shift-dragging for Raw on tier
 *     1 and then escalating carries that intent across;
 *   * the text most recently produced from them, so the daemon can say whether
 *     the escalation changed the clipboard or merely spent 535 ms.
 *
 * Pure and free of Qt signals on purpose: everything worth asserting about the
 * escalation lives here, where it is testable without a compositor.
 */
class Tier2Cache
{
public:
    /// True once a crop has been stored. A cancelled drag must not clear one --
    /// cancelling a new selection should not destroy the last good one.
    bool hasCrop() const { return !m_crop.isNull(); }

    /// Replaces the whole entry. Called on every path, tier 1 and tier 2 alike,
    /// so a tier-2 result is itself re-escalatable.
    void store(const QImage &crop,
               std::optional<LayoutKind> forcedKind,
               const QString &text);

    const QImage &crop() const { return m_crop; }
    std::optional<LayoutKind> forcedKind() const { return m_forcedKind; }

    /// The text last produced from this crop -- NOT specifically tier 1's.
    /// The tier-2 path overwrites it, so a second escalation compares tier 2
    /// against tier 2, which is the right question.
    const QString &previousText() const { return m_previousText; }

    /// True if `text` differs from what is stored, i.e. this re-read changed
    /// the clipboard.
    bool changesText(const QString &text) const
    {
        return text != m_previousText;
    }

private:
    QImage                    m_crop;
    std::optional<LayoutKind> m_forcedKind;
    QString                   m_previousText;
};

} // namespace textract
