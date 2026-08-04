// SPDX-FileCopyrightText: 2026 gzrx
// SPDX-License-Identifier: GPL-3.0-or-later

#include <LayerShellQt/Shell>

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KGlobalAccel>
#include <KSharedConfig>

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QKeySequence>
#include <QMessageBox>
#include <QTextStream>

#include "app/extractorcontroller.h"
#include "capture/kwincapture.h"
#include "config/configdialog.h"
#include "config/settings.h"
#include "models/fetch.h"
#include "ocr/tesseractengine.h"
#include "overlay/selectionoverlay.h"

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
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

    QCommandLineOption configureOption(
        QStringLiteral("configure"),
        QStringLiteral("Open the settings dialog and exit."));
    parser.addOption(configureOption);

    parser.process(app);

    QTextStream err(stderr);

    // main.cpp is the only place that opens KSharedConfig. config/settings.cpp
    // is pure over whatever group it is handed, and nothing a test links reads
    // config at all -- which is what keeps a local config file from being able
    // to move a corpus score.
    KSharedConfig::Ptr config = KSharedConfig::openConfig(
        QStringLiteral("textractrc"), KConfig::SimpleConfig);
    textract::Settings settings = textract::loadSettings(config->group(QString()));

    if (parser.isSet(configureOption)) {
        if (!textract::runConfigDialog(&settings, textract::availableLanguages())) {
            return 0; // cancelled; nothing written
        }

        // Notify is what makes a running daemon reload. Without it the file is
        // updated correctly and KConfigWatcher never emits configChanged(),
        // which looks exactly like the watcher never being wired up.
        KConfigGroup root = config->group(QString());
        textract::saveSettings(root, settings, KConfigBase::Notify);
        if (!config->sync()) {
            // --configure is normally launched from a desktop entry or KRunner,
            // where stderr goes nowhere. Without this the dialog just vanishes
            // and a failed write is indistinguishable from a successful one.
            QMessageBox::critical(
                nullptr, QStringLiteral("Could not save settings"),
                QStringLiteral("Failed to write %1.").arg(config->name()));
            err << "could not write " << config->name() << "\n";
            return 1;
        }
        QTextStream(stdout) << "Settings saved to " << config->name() << "\n";
        return 0;
    }

    if (parser.isSet(fetchModelsOption)) {
        // No compositor, no overlay, no ScreenShot2 authorisation needed. The
        // QApplication above is already constructed and is left alone rather
        // than restructured; this branch simply uses none of it.
        //
        // The configured directory, not the default: a user who moved their
        // models and then runs the repair tool means the directory they moved
        // them to. Fetching into the default while the daemon reads elsewhere
        // would report "models not installed" right after a successful fetch.
        const QString dir = textract::resolveModelDir(settings);
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

        // warmUp() before applySettings(), deliberately. warmUp() sets m_langs,
        // so applySettings()'s language branch then compares equal and does
        // nothing, leaving it to apply only the preprocessing options and the
        // model directory. The other order double-initialises: applySettings()
        // would run its own recovery for a bad language, and the warmUp() after
        // it would tear the recovered engine straight back down.
        //
        // A language that will not load is fatal HERE and non-fatal in
        // applySettings(), and that asymmetry is intended: at startup there is
        // no working engine to fall back to, but in a running daemon there is.
        if (!controller->warmUp(settings.langs)) {
            err << "could not load tesseract data for '" << settings.langs
                << "'; install the tesseract-data package for it\n";
            return 1;
        }
        controller->applySettings(settings);

        auto *action = new QAction(&app);
        action->setObjectName(QStringLiteral("extract_text"));
        action->setText(QStringLiteral("Extract text from screen region"));
        QObject::connect(action, &QAction::triggered,
                         controller, &textract::ExtractorController::extract);

        // Meta+X rather than a dedicated media key. XF86Calculator is absent
        // from most compact and laptop keyboards, so the previous default left
        // those users pressing a key they do not have: no capture, no error,
        // nothing in the journal. Meta+X was checked against a live
        // kglobalshortcutsrc and collides with no Plasma binding. Meta+T, the
        // original choice before Calculator, is Plasma's Edit Tiles.
        //
        // Tier 1 deliberately carries NO Shift, and that is a correctness
        // constraint rather than a preference. Shift held through the drag
        // means "force Raw", and SelectionOverlay accumulates modifiers with
        // |= across press, move and release -- so a Shift-bearing default would
        // latch Raw for any user who presses the shortcut and drags straight
        // away, silently, on every capture.
        const QList<QKeySequence> shortcut{
            QKeySequence(Qt::MetaModifier | Qt::Key_X)};

        // NoAutoloading on setDefaultShortcut pins what this build considers the
        // default. setShortcut deliberately does NOT pass it, so a binding the user
        // set in System Settings is loaded and wins.
        //
        // This reverses an earlier decision, and the trap it was avoiding comes
        // back -- for developers only. Once a binding is stored, changing the
        // QKeySequence below appears to do nothing. Clear it with:
        //   kwriteconfig6 --file kglobalshortcutsrc --group textract \
        //                 --key extract_text --delete
        KGlobalAccel::self()->setDefaultShortcut(action, shortcut,
                                                 KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(action, shortcut);

        auto *tier2Action = new QAction(&app);
        tier2Action->setObjectName(QStringLiteral("extract_text_tier2"));
        tier2Action->setText(QStringLiteral("Extract text (tier 2, PP-OCRv6)"));
        QObject::connect(tier2Action, &QAction::triggered,
                         controller, &textract::ExtractorController::extractTier2);

        // Meta+Shift+X: tier 1's key plus Shift, keeping the pairing the
        // Calculator bindings had. Shift also means "force Raw" when held
        // through a drag, but those are different moments -- a shortcut chord
        // versus a mouse drag -- and the tier-2 path that does open the overlay
        // passes std::nullopt unconditionally rather than re-reading the
        // modifier. See ExtractorController::onSelected().
        const QList<QKeySequence> tier2Shortcut{
            QKeySequence(Qt::MetaModifier | Qt::ShiftModifier | Qt::Key_X)};

        // Same NoAutoloading asymmetry as extract_text above, for the same
        // reason; the stored-binding remedy there applies here with
        // --key extract_text_tier2.
        KGlobalAccel::self()->setDefaultShortcut(tier2Action, tier2Shortcut,
                                                 KGlobalAccel::NoAutoloading);
        KGlobalAccel::self()->setShortcut(tier2Action, tier2Shortcut);

        // Live reload. KConfigWatcher signals on an external write, which is
        // what `textract --configure` performs from a separate process.
        KConfigWatcher::Ptr watcher = KConfigWatcher::create(config);
        QObject::connect(watcher.data(), &KConfigWatcher::configChanged,
                         controller, [config, controller](const KConfigGroup &,
                                                          const QByteArrayList &) {
            controller->applySettings(
                textract::loadSettings(config->group(QString())));
        });

        QTextStream(stdout)
            << "textract daemon ready; Meta+X = tier 1, "
               "Meta+Shift+X = tier 2 (defaults)\n";
        return app.exec();
    }

    err << "nothing to do; see --help\n";
    return 1;
}
