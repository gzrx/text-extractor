// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <LayerShellQt/Shell>

#include <KGlobalAccel>

#include <QAction>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTextStream>

#include "app/extractorcontroller.h"
#include "capture/kwincapture.h"
#include "models/fetch.h"
#include "ocr/onnxpaddleengine.h"
#include "overlay/selectionoverlay.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("textract"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    // Must run before any window is created: this swaps the Wayland shell
    // integration, and windows made beforehand get the wrong surface type.
    LayerShellQt::Shell::useLayerShell();

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Extract text from a screen region"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption captureTest(
        QStringLiteral("capture-test"),
        QStringLiteral("Capture the workspace to <file> and exit."),
        QStringLiteral("file"));
    parser.addOption(captureTest);

    QCommandLineOption selectTest(
        QStringLiteral("select-test"),
        QStringLiteral("Capture, let the user drag, save the crop to <file>."),
        QStringLiteral("file"));
    parser.addOption(selectTest);

    QCommandLineOption daemonMode(
        QStringLiteral("daemon"),
        QStringLiteral("Stay resident and listen for the global shortcut."));
    parser.addOption(daemonMode);

    QCommandLineOption fetchModelsOption(
        QStringLiteral("fetch-models"),
        QStringLiteral("Download the tier-2 PP-OCRv6 models and exit."));
    parser.addOption(fetchModelsOption);

    parser.process(app);

    QTextStream err(stderr);

    if (parser.isSet(fetchModelsOption)) {
        // No compositor, no overlay, no ScreenShot2 authorisation needed. The
        // QGuiApplication above is already constructed and is left alone rather
        // than restructured; this branch simply uses none of it.
        const QString dir = textract::OnnxPaddleEngine::defaultModelDir();
        QTextStream(stdout) << "Fetching tier-2 models into " << dir << "\n";

        QString error;
        if (!textract::fetchModels(dir, &error)) {
            err << "fetch failed: " << error << "\n";
            return 1;
        }
        QTextStream(stdout) << "All four models installed and verified.\n";
        return 0;
    }

    if (parser.isSet(captureTest)) {
        QString error;
        const textract::CaptureResult result = textract::captureWorkspace(&error);
        if (!result.isValid()) {
            err << "capture failed: " << error << "\n";
            return 1;
        }
        const QString path = parser.value(captureTest);
        if (!result.image.save(path)) {
            err << "could not write " << path << "\n";
            return 1;
        }
        QTextStream(stdout)
            << "wrote " << path << " "
            << result.image.width() << "x" << result.image.height()
            << " scale=" << result.scale << "\n";
        return 0;
    }

    if (parser.isSet(selectTest)) {
        QString error;
        const textract::CaptureResult result = textract::captureWorkspace(&error);
        if (!result.isValid()) {
            err << "capture failed: " << error << "\n";
            return 1;
        }

        const QString path = parser.value(selectTest);
        auto *overlay = new textract::SelectionOverlay(&app);
        int exitCode = 0;

        QObject::connect(overlay, &textract::SelectionOverlay::selected,
                         &app, [&, path](const QRect &rect) {
            const QImage crop = result.image.copy(rect);
            QTextStream(stdout)
                << "selected " << rect.x() << "," << rect.y() << " "
                << rect.width() << "x" << rect.height() << "\n";
            if (!crop.save(path)) {
                QTextStream(stderr) << "could not write " << path << "\n";
                exitCode = 1;
            }
            app.quit();
        });
        QObject::connect(overlay, &textract::SelectionOverlay::cancelled,
                         &app, [&app, &exitCode] {
            QTextStream(stdout) << "cancelled\n";
            exitCode = 0;
            app.quit();
        });

        overlay->start(result.image);
        app.exec();
        return exitCode;
    }

    if (parser.isSet(daemonMode)) {
        auto *controller = new textract::ExtractorController(&app);
        if (!controller->warmUp(QStringLiteral("eng"))) {
            err << "could not load tesseract 'eng' data; "
                   "install tesseract-data-eng\n";
            return 1;
        }

        auto *action = new QAction(&app);
        action->setObjectName(QStringLiteral("extract_text"));
        action->setText(QStringLiteral("Extract text from screen region"));
        QObject::connect(action, &QAction::triggered,
                         controller, &textract::ExtractorController::extract);

        // The Calculator key (XF86Calculator) rather than a modifier combo:
        // it is a dedicated key on this keyboard and does not collide with the
        // existing Plasma shortcuts.
        const QList<QKeySequence> shortcut{QKeySequence(Qt::Key_Calculator)};

        // NoAutoloading forces this binding instead of whatever KGlobalAccel
        // has stored for the component from a previous run. Without it, a
        // shortcut changed in code is silently ignored in favour of the
        // persisted one in kglobalshortcutsrc.
        KGlobalAccel::self()->setDefaultShortcut(action, shortcut,
                                                 KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(action, shortcut,
                                          KGlobalAccel::NoAutoloading);

        auto *tier2Action = new QAction(&app);
        tier2Action->setObjectName(QStringLiteral("extract_text_tier2"));
        tier2Action->setText(QStringLiteral("Extract text (tier 2, PP-OCRv6)"));
        QObject::connect(tier2Action, &QAction::triggered,
                         controller, &textract::ExtractorController::extractTier2);

        // Shift+Calculator. Shift also means "force Raw" when held through the
        // drag, but those are different moments -- a shortcut chord versus a
        // mouse drag -- and the tier-2 path that does open the overlay
        // deliberately ignores the modifier. See ExtractorController::onSelected().
        const QList<QKeySequence> tier2Shortcut{
            QKeySequence(Qt::ShiftModifier | Qt::Key_Calculator)};

        KGlobalAccel::self()->setDefaultShortcut(tier2Action, tier2Shortcut,
                                                 KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(tier2Action, tier2Shortcut,
                                          KGlobalAccel::NoAutoloading);

        QTextStream(stdout)
            << "textract daemon ready; Calculator = tier 1, "
               "Shift+Calculator = tier 2\n";
        return app.exec();
    }

    err << "nothing to do; see --help\n";
    return 1;
}
