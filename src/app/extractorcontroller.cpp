// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include "app/extractorcontroller.h"

#include "assemble/assemble.h"
#include "capture/kwincapture.h"
#include "clipboard/clipboard.h"
#include "overlay/selectionoverlay.h"

#include <KNotification>

#include <QDebug>

namespace textract {

namespace {
/// Below this mean confidence the user is told the result may be poor.
constexpr float kLowConfidence = 0.70f;
} // namespace

ExtractorController::ExtractorController(QObject *parent)
    : QObject(parent)
    , m_overlay(new SelectionOverlay(this))
{
    connect(m_overlay, &SelectionOverlay::selected,
            this, &ExtractorController::onSelected);
    connect(m_overlay, &SelectionOverlay::cancelled, this, [this] {
        m_busy = false;
        m_workspace = QImage();
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
    m_workspace = result.image;
    m_overlay->start(m_workspace);
}

void ExtractorController::onSelected(const QRect &physicalRect)
{
    m_busy = false;

    const QImage crop = m_workspace.copy(physicalRect);
    m_workspace = QImage();

    const std::vector<Word> words = m_engine.recognize(crop, m_langs);
    if (words.empty()) {
        // Clipboard deliberately left untouched.
        notify(QStringLiteral("No text found"),
               QStringLiteral("Nothing was recognised in that region."));
        return;
    }

    const QString text = assemble(words, LayoutKind::Raw);
    if (text.isEmpty()) {
        notify(QStringLiteral("No text found"),
               QStringLiteral("Nothing was recognised in that region."));
        return;
    }

    copyToClipboard(text);

    float total = 0.0f;
    for (const Word &word : words) {
        total += word.confidence;
    }
    const float mean = total / float(words.size());

    if (mean < kLowConfidence) {
        notify(QStringLiteral("Copied with low confidence"),
               QStringLiteral("Mean confidence %1%. The result may contain errors.")
                   .arg(int(mean * 100)));
    }
}

void ExtractorController::notify(const QString &title, const QString &body)
{
    // The StandardEvent overload works without a .notifyrc file. The
    // eventId-based constructor silently shows nothing unless the application
    // ships and installs a notification config, which M2 does not.
    KNotification::event(KNotification::Notification, title, body);
}

} // namespace textract
