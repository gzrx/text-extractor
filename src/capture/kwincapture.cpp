#include "capture/kwincapture.h"
#include "capture/rawimage.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingReply>
#include <QDBusUnixFileDescriptor>
#include <QVariantMap>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace textract {

namespace {

constexpr auto kService   = "org.kde.KWin.ScreenShot2";
constexpr auto kPath      = "/org/kde/KWin/ScreenShot2";
constexpr auto kInterface = "org.kde.KWin.ScreenShot2";

/// Reads from `fd` until EOF. Takes ownership of `fd` and closes it.
QByteArray drainPipe(int fd)
{
    QByteArray out;
    char buffer[64 * 1024];
    for (;;) {
        const ssize_t n = ::read(fd, buffer, sizeof(buffer));
        if (n > 0) {
            out.append(buffer, int(n));
        } else if (n == 0) {
            break; // EOF
        } else if (errno != EINTR) {
            break; // real error; caller detects via size validation
        }
    }
    ::close(fd);
    return out;
}

} // namespace

CaptureResult captureWorkspace(QString *error)
{
    const auto fail = [error](const QString &message) {
        if (error) {
            *error = message;
        }
        return CaptureResult{};
    };

    int fds[2];
    if (::pipe2(fds, O_CLOEXEC) != 0) {
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

    // Dispatch asynchronously BEFORE reading, then close our copy of the write
    // end so that the pipe reaches EOF once KWin is done.
    QDBusPendingCall pending =
        QDBusConnection::sessionBus().asyncCall(message);
    ::close(writeFd);

    // Drain while the call is in flight. Doing this after waitForFinished()
    // would deadlock on any image larger than the 64KB pipe buffer.
    const QByteArray bytes = drainPipe(readFd);

    pending.waitForFinished();
    const QDBusPendingReply<QVariantMap> reply = pending;
    if (reply.isError()) {
        const QString name = reply.error().name();
        if (name.contains(QLatin1String("NoAuthorized"))) {
            return fail(QStringLiteral(
                "KWin refused the screenshot: this binary is not authorised. "
                "Ensure org.kde.textract.desktop is installed and contains "
                "X-KDE-DBUS-Restricted-Interfaces=org.kde.kwin.screenshot"));
        }
        return fail(QStringLiteral("ScreenShot2 call failed: %1: %2")
                        .arg(name, reply.error().message()));
    }

    const QVariantMap results = reply.value();
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
            "Received %1 bytes but could not decode them as an image")
                        .arg(bytes.size()));
    }

    out.scale = results.value(QStringLiteral("scale"), 1.0).toDouble();
    return out;
}

} // namespace textract
