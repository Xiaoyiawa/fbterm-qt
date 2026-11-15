#include "consolemode.h"

#include <QDebug>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <unistd.h>

void ConsoleMode::enterGraphics()
{
    if (ttyFd_ >= 0) return;
    ttyFd_ = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (ttyFd_ < 0) {
        qWarning() << "cannot open /dev/tty0:" << strerror(errno);
        return;
    }
    if (ioctl(ttyFd_, KDSETMODE, KD_GRAPHICS) != 0) {
        qWarning() << "KDSETMODE(KD_GRAPHICS) failed:" << strerror(errno);
    } else {
        graphics_ = true;
        qInfo() << "framebuffer console switched to graphics mode";
    }
}

ConsoleMode::~ConsoleMode()
{
    if (ttyFd_ >= 0) {
        if (graphics_) {
            if (ioctl(ttyFd_, KDSETMODE, KD_TEXT) != 0)
                qWarning() << "KDSETMODE(KD_TEXT) failed:" << strerror(errno);
            else
                qInfo() << "framebuffer console restored to text mode";
        }
        close(ttyFd_);
        ttyFd_ = -1;
    }
}
