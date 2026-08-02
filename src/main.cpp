#include <QCommandLineParser>
#include <QGuiApplication>
#include <QTextStream>

#include "capture/kwincapture.h"

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("textract"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

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

    err << "nothing to do; see --help\n";
    return 1;
}
