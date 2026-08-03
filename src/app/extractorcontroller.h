// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>

#include <QImage>
#include <QObject>
#include <QRect>

#include "app/tier2cache.h"
#include "config/settings.h"
#include "ocr/onnxpaddleengine.h"
#include "ocr/tesseractengine.h"
#include "preprocess/preprocess.h"

namespace textract {

class SelectionOverlay;

/// Owns the warm OCR engines and drives one capture-to-clipboard cycle.
class ExtractorController : public QObject
{
    Q_OBJECT
public:
    explicit ExtractorController(QObject *parent = nullptr);

    /// Loads the OCR language data up front so the hotkey path stays fast.
    /// Tier 2 is deliberately NOT warmed here -- see ensureTier2Engine().
    bool warmUp(const QString &langs);

    /// Applies changed settings to a running daemon. Safe to call with settings
    /// identical to the current ones -- each case is guarded by a comparison.
    void applySettings(const Settings &settings);

    /// Tier 1: capture, select, recognise with Tesseract, copy.
    void extract();

    /// Tier 2: re-run the cached crop through PP-OCRv6, or capture one first
    /// if nothing is cached yet.
    void extractTier2();

private:
    void onSelected(const QRect &physicalRect,
                    Qt::KeyboardModifiers modifiers);

    /// Recognises `crop` with tier 2 and reports whether it changed anything.
    void runTier2(const QImage &crop, std::optional<LayoutKind> forcedKind);

    /// The tier-2 engine, or nullptr with `*title` and `*body` set to a message
    /// naming the actual remedy.
    OnnxPaddleEngine *ensureTier2Engine(QString *title, QString *body);

    void notify(const QString &title, const QString &body);

    TesseractEngine                   m_engine;
    std::unique_ptr<OnnxPaddleEngine> m_paddle;
    Tier2Cache                        m_tier2Cache;
    SelectionOverlay                 *m_overlay{nullptr};
    QImage                            m_workspace;
    PreprocessOptions                 m_preprocess{};
    QString                           m_langs{QStringLiteral("eng")};
    /// Resolved via resolveModelDir(), so TEXTRACT_MODELS still wins.
    QString                           m_modelDir{OnnxPaddleEngine::defaultModelDir()};
    bool                              m_busy{false};

    /// Set while a drag started by extractTier2() is on screen, so the one
    /// selected() handler knows which tier asked for it.
    bool m_tier2Pending{false};
};

} // namespace textract
