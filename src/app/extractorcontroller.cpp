// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/extractorcontroller.h"

#include "analyze/analyze.h"
#include "app/extraction.h"
#include "models/modelspec.h"

#include "capture/kwincapture.h"
#include "clipboard/clipboard.h"
#include "overlay/selectionoverlay.h"

#include <KNotification>

#include <QDebug>

namespace textract {

namespace {
/// Below this mean confidence the user is told the result may be poor.
constexpr float kLowConfidence = 0.70f;

QString layoutName(LayoutKind kind)
{
    switch (kind) {
    case LayoutKind::Raw:   return QStringLiteral("Raw");
    case LayoutKind::Code:  return QStringLiteral("Code");
    case LayoutKind::Prose: return QStringLiteral("Prose");
    case LayoutKind::Table: return QStringLiteral("Table");
    }
    return QStringLiteral("Raw");
}
} // namespace

ExtractorController::ExtractorController(QObject *parent)
    : QObject(parent)
    , m_overlay(new SelectionOverlay(this))
{
    connect(m_overlay, &SelectionOverlay::selected,
            this, &ExtractorController::onSelected);
    connect(m_overlay, &SelectionOverlay::cancelled, this, [this] {
        m_busy = false;
        m_tier2Pending = false;
        m_workspace = QImage();
        // m_tier2Cache is deliberately NOT cleared: cancelling a new selection
        // should not destroy the last good one.
    });
}

bool ExtractorController::warmUp(const QString &langs)
{
    m_langs = langs;
    if (!m_engine.initialize(langs)) {
        qWarning() << "failed to load tesseract data for" << langs;
        return false;
    }
    return true;
}

void ExtractorController::extract()
{
    if (m_busy) {
        return; // a selection is already on screen
    }

    QString error;
    const CaptureResult result = captureWorkspace(&error);
    if (!result.isValid()) {
        notify(QStringLiteral("Capture failed"), error);
        return;
    }

    m_busy = true;
    m_tier2Pending = false;
    m_workspace = result.image;
    m_overlay->start(m_workspace);
}

void ExtractorController::extractTier2()
{
    if (m_busy) {
        return;
    }

    QString title;
    QString body;
    if (!ensureTier2Engine(&title, &body)) {
        notify(title, body);
        return;
    }

    // The main path: the same pixels and the same declared intent. No capture,
    // no overlay, no re-drag -- an escalation that also changed the region
    // would be comparing two different things.
    if (m_tier2Cache.hasCrop()) {
        runTier2(m_tier2Cache.crop(), m_tier2Cache.forcedKind());
        return;
    }

    // Nothing cached yet, so fall back to capturing one, then recognising it
    // with tier 2 rather than tier 1.
    QString error;
    const CaptureResult result = captureWorkspace(&error);
    if (!result.isValid()) {
        notify(QStringLiteral("Capture failed"), error);
        return;
    }

    m_busy = true;
    m_tier2Pending = true;
    m_workspace = result.image;
    m_overlay->start(m_workspace);
}

void ExtractorController::onSelected(const QRect &physicalRect,
                                     Qt::KeyboardModifiers modifiers)
{
    m_busy = false;
    const bool tier2 = m_tier2Pending;
    m_tier2Pending = false;

    const QImage crop = m_workspace.copy(physicalRect);
    m_workspace = QImage();

    if (tier2) {
        // forcedKind is unconditionally nullopt here. Shift was consumed by the
        // Shift+Calculator shortcut and is NOT re-read as force-Raw:
        // SelectionOverlay accumulates modifiers with |= across press, move and
        // release, so a user still holding Shift as the drag begins would
        // otherwise silently pin Raw without having asked for it. Raw stays
        // reachable on tier 2 by Shift-dragging on Calculator first, which
        // caches the forced kind, and then pressing Shift+Calculator.
        runTier2(crop, std::nullopt);
        return;
    }

    // Normally no forced kind: extractText() classifies, behind the seam the
    // fixture harness also runs, so what is measured is what the daemon does.
    // Shift held during the drag overrides it -- every heuristic is wrong
    // sometimes, and the user should be able to take it back on the next
    // attempt rather than go looking for a setting.
    const std::optional<LayoutKind> forcedKind = forcedLayoutFor(modifiers);
    const Extraction result = extractText(m_engine, crop, m_langs,
                                          forcedKind, m_preprocess);

    // Cached even when tier 1 recognised nothing. That case is the strongest
    // reason to escalate -- tier 2 reading text where tier 1 read none is the
    // feature working -- so discarding the crop there would be the one moment
    // the user most wants the second key.
    m_tier2Cache.store(crop, forcedKind, result.text);

    if (result.isEmpty() || result.text.isEmpty()) {
        // Clipboard deliberately left untouched.
        notify(QStringLiteral("No text found"),
               QStringLiteral("Nothing was recognised in that region."));
        return;
    }

    copyToClipboard(result.text);

    if (result.meanConfidence < kLowConfidence) {
        notify(QStringLiteral("Copied with low confidence"),
               QStringLiteral("Mean confidence %1%. The result may contain errors.")
                   .arg(int(result.meanConfidence * 100)));
    }
}

void ExtractorController::runTier2(const QImage &crop,
                                   std::optional<LayoutKind> forcedKind)
{
    QString title;
    QString body;
    OnnxPaddleEngine *engine = ensureTier2Engine(&title, &body);
    if (!engine) {
        notify(title, body);
        return;
    }

    // The same PreprocessOptions as tier 1. HANDOFF 13.6 measured tier 2's own
    // sweep preferring 2x by 0.0019, below this project's 0.002 significance
    // threshold, so a separate options set would be tuning against noise.
    const Extraction result = extractText(*engine, crop, m_langs,
                                          forcedKind, m_preprocess);

    if (result.isEmpty() || result.text.isEmpty()) {
        m_tier2Cache.store(crop, forcedKind, QString());
        notify(QStringLiteral("No text found"),
               QStringLiteral("Tier 2 recognised nothing in that region."));
        return;
    }

    const bool changed =
        !m_tier2Cache.hasCrop() || m_tier2Cache.changesText(result.text);

    copyToClipboard(result.text);
    m_tier2Cache.store(crop, forcedKind, result.text);

    // One press, one message: this replaces the low-confidence notification on
    // tier 2 rather than stacking with it. Little is lost -- HANDOFF 13.5
    // measured tier-2 confidences at 0.94-1.00, so the 0.70 threshold would
    // essentially never fire on this path.
    //
    // Both branches report the layout. The unchanged branch needs it just as
    // much: it is the only way to see that a forced kind cached from a
    // Shift-drag on tier 1 carried across the escalation, and a message that
    // dropped it made that case unobservable from the notification alone.
    if (changed) {
        notify(QStringLiteral("Re-read with tier 2"),
               QStringLiteral("Clipboard replaced. Layout: %1.")
                   .arg(layoutName(result.kind)));
    } else {
        notify(QStringLiteral("Tier 2 read it the same way"),
               QStringLiteral("Clipboard unchanged. Layout: %1.")
                   .arg(layoutName(result.kind)));
    }
}

OnnxPaddleEngine *ExtractorController::ensureTier2Engine(QString *title,
                                                         QString *body)
{
    const QString dir = OnnxPaddleEngine::defaultModelDir();

    // The order of these two checks is what keeps the two remedies apart, and
    // getting it backwards reports "not installed" for a corrupt model and
    // sends the user to the wrong fix. That is the mistake HANDOFF 3.1a already
    // cost this project once, in a different module.
    //
    // 1. Missing files are checked BEFORE construction.
    const QStringList missing = modelsMissing(dir);
    if (!missing.isEmpty()) {
        *title = QStringLiteral("Tier 2 models not installed");
        *body = QStringLiteral("Missing %1 in %2.\nRun: textract --fetch-models")
                    .arg(missing.join(QStringLiteral(", ")), dir);
        return nullptr;
    }

    if (!m_paddle) {
        // ~290 ms of session construction (HANDOFF 12.2), paid once on the
        // first tier-2 press rather than at daemon start, and kept alive
        // afterwards. Constructing per keypress is explicitly ruled out there;
        // constructing eagerly would charge a tier-1-only user 290 ms and
        // ~31 MB for a feature they never use.
        m_paddle = std::make_unique<OnnxPaddleEngine>(dir);
    }

    // 2. available() is consulted only AFTER construction, and only when
    //    nothing was missing -- so false here means the files are all present
    //    and one of them is bad. This is the charset guard from HANDOFF 13.3
    //    firing, and it is worth having only if the message says "corrupt"
    //    rather than "not installed".
    if (!m_paddle->available()) {
        *title = QStringLiteral("Tier 2 models failed to load");
        *body = QStringLiteral("The models in %1 are present but did not load; "
                               "they may be corrupt.\n"
                               "Re-run: textract --fetch-models")
                    .arg(dir);
        return nullptr;
    }

    return m_paddle.get();
}

void ExtractorController::notify(const QString &title, const QString &body)
{
    // The StandardEvent overload works without a .notifyrc file. The
    // eventId-based constructor silently shows nothing unless the application
    // ships and installs a notification config, which M2 does not.
    KNotification::event(KNotification::Notification, title, body);
}

} // namespace textract
