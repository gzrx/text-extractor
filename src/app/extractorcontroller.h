#pragma once

#include <QImage>
#include <QObject>
#include <QRect>

#include "ocr/tesseractengine.h"

namespace textract {

class SelectionOverlay;

/// Owns the warm OCR engine and drives one capture-to-clipboard cycle.
class ExtractorController : public QObject
{
    Q_OBJECT
public:
    explicit ExtractorController(QObject *parent = nullptr);

    /// Loads the OCR language data up front so the hotkey path stays fast.
    bool warmUp(const QString &langs);

    /// Runs a full cycle: capture, select, recognise, copy.
    void extract();

private:
    void onSelected(const QRect &physicalRect);
    void notify(const QString &title, const QString &body);

    TesseractEngine   m_engine;
    SelectionOverlay *m_overlay{nullptr};
    QImage            m_workspace;
    QString           m_langs{QStringLiteral("eng")};
    bool              m_busy{false};
};

} // namespace textract
