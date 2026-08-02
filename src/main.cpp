#include <LayerShellQt/Shell>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTextStream>

#include "capture/kwincapture.h"
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

    parser.process(app);

    QTextStream err(stderr);

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

    err << "nothing to do; see --help\n";
    return 1;
}
