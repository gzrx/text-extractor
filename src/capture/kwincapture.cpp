#include "capture/kwincapture.h"
#include "capture/rawimage.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QEventLoop>
#include <QSocketNotifier>
#include <QTimer>
#include <QVariantMap>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace textract {

namespace {

constexpr auto kService   = "org.kde.KWin.ScreenShot2";
constexpr auto kPath      = "/org/kde/KWin/ScreenShot2";
constexpr auto kInterface = "org.kde.KWin.ScreenShot2";

/// Give up rather than hang if KWin never answers.
constexpr int kTimeoutMs = 10000;

} // namespace

CaptureResult captureWorkspace(QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return CaptureResult{};
    };

    // O_NONBLOCK matters: the pipe is drained from an event-loop callback,
    // which must never block the loop that is also delivering the D-Bus reply.
    int fds[2];
    if (::pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0) {
        return fail(QStringLiteral("Failed to create pipe"));
    }
    const int readFd  = fds[0];
    const int writeFd = fds[1];

    QVariantMap options;
    options.insert(QStringLiteral("native-resolution"), true);
    options.insert(QStringLiteral("include-cursor"), false);

    QDBusMessage message = QDBusMessage::createMethodCall(
        QLatin1String(kService),
        QLatin1String(kPath),
        QLatin1String(kInterface),
        QStringLiteral("CaptureWorkspace"));
    message.setArguments({options,
                          QVariant::fromValue(QDBusUnixFileDescriptor(writeFd))});

    QDBusPendingCall pending = QDBusConnection::sessionBus().asyncCall(message);
    ::close(writeFd);

    QByteArray  bytes;
    QVariantMap results;
    quint64     expected{0};
    bool        replyDone{false};
    bool        replyFailed{false};
    QString     replyError;
    bool        eof{false};
    bool        timedOut{false};

    QEventLoop loop;

    // Finish once the reply has arrived AND the pixel data is complete.
    //
    // Completion is decided by byte count, not by EOF. QDBusUnixFileDescriptor
    // duplicates the descriptor it is given, so this process retains a write
    // end of the pipe for as long as the message is alive; waiting for EOF
    // would therefore block forever even after KWin has written everything.
    const auto maybeQuit = [&] {
        if (replyFailed) {
            loop.quit();
            return;
        }
        if (!replyDone) {
            return;
        }
        if (eof || (expected > 0 && quint64(bytes.size()) >= expected)) {
            loop.quit();
        }
    };

    QSocketNotifier notifier(readFd, QSocketNotifier::Read);
    QObject::connect(&notifier, &QSocketNotifier::activated, &loop, [&] {
        char buffer[64 * 1024];
        for (;;) {
            const ssize_t n = ::read(readFd, buffer, sizeof(buffer));
            if (n > 0) {
                bytes.append(buffer, int(n));
                continue;
            }
            if (n == 0) {
                eof = true;
                notifier.setEnabled(false);
                break;
            }
            if (errno == EINTR) {
                continue;
            }
            break; // EAGAIN: nothing more available right now
        }
        maybeQuit();
    });

    QDBusPendingCallWatcher watcher(pending);
    QObject::connect(&watcher, &QDBusPendingCallWatcher::finished, &loop, [&] {
        const QDBusPendingReply<QVariantMap> reply = watcher;
        if (reply.isError()) {
            replyFailed = true;
            const QString name = reply.error().name();
            if (name.contains(QLatin1String("NoAuthorized"))) {
                replyError = QStringLiteral(
                    "KWin refused the screenshot: this binary is not "
                    "authorised.\n"
                    "An installed .desktop file must contain "
                    "X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2 "
                    "(case-sensitive) and an Exec= line whose absolute path is "
                    "exactly this binary:\n  %1\n"
                    "For a build-tree binary, run:\n"
                    "  cp build/org.kde.textract.dev.desktop "
                    "~/.local/share/applications/")
                                 .arg(QCoreApplication::applicationFilePath());
            } else {
                replyError = QStringLiteral("ScreenShot2 call failed: %1: %2")
                                 .arg(name, reply.error().message());
            }
        } else {
            results = reply.value();
            expected = quint64(results.value(QStringLiteral("stride")).toUInt())
                     * quint64(results.value(QStringLiteral("height")).toUInt());
            replyDone = true;
        }
        maybeQuit();
    });

    QTimer::singleShot(kTimeoutMs, &loop, [&] {
        timedOut = true;
        loop.quit();
    });

    loop.exec();

    notifier.setEnabled(false);
    ::close(readFd);

    if (replyFailed) {
        return fail(replyError);
    }
    if (timedOut) {
        return fail(QStringLiteral(
            "Timed out after %1 ms waiting for KWin (received %2 bytes)")
                        .arg(kTimeoutMs)
                        .arg(bytes.size()));
    }
    if (results.value(QStringLiteral("type")).toString()
        != QLatin1String("raw")) {
        return fail(QStringLiteral("Unsupported screenshot type"));
    }

    CaptureResult out;
    out.image = imageFromRaw(bytes,
                             results.value(QStringLiteral("width")).toUInt(),
                             results.value(QStringLiteral("height")).toUInt(),
                             results.value(QStringLiteral("stride")).toUInt(),
                             results.value(QStringLiteral("format")).toUInt());
    if (out.image.isNull()) {
        return fail(QStringLiteral(
            "Received %1 bytes but could not decode them as an image "
            "(expected %2)")
                        .arg(bytes.size())
                        .arg(expected));
    }

    out.scale = results.value(QStringLiteral("scale"), 1.0).toDouble();
    return out;
}

} // namespace textract
