#include "ui/display.h"

#include "app/log.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/kd.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace lvfb {

Display::Display() = default;

Display::~Display()
{
    shutdown();
}

bool Display::init(const std::string& fbPath, std::string* error)
{
    lv_init();
    lvInitialized_ = true;

    // HiFB may start with the panel blanked. Try to unblank before LVGL maps
    // the framebuffer; failures are non-fatal.
    const int fbFd = open(fbPath.c_str(), O_RDWR | O_CLOEXEC);
    if (fbFd >= 0) {
        if (ioctl(fbFd, FBIOBLANK, FB_BLANK_UNBLANK) != 0) {
            LVFB_LOGD("FBIOBLANK unblank failed: %s", std::strerror(errno));
        }
        close(fbFd);
    }

    display_ = lv_linux_fbdev_create();
    if (display_ == nullptr) {
        if (error != nullptr) {
            *error = "lv_linux_fbdev_create failed";
        }
        return false;
    }

    // We already attempted the unblank ourselves; avoid a duplicate ioctl
    // and its warning on drivers that do not implement FBIOBLANK.
    lv_linux_fbdev_set_skip_unblank(display_, true);

    const lv_result_t result = lv_linux_fbdev_set_file(display_, fbPath.c_str());
    if (result != LV_RESULT_OK) {
        if (error != nullptr) {
            *error = "cannot initialize framebuffer " + fbPath;
        }
        return false;
    }

    fbFd_ = open(fbPath.c_str(), O_RDWR | O_CLOEXEC);
    if (fbFd_ >= 0) {
        if (ioctl(fbFd_, FBIOGET_VSCREENINFO, &vinfo_) != 0) {
            LVFB_LOGW("FBIOGET_VSCREENINFO on refresh fd failed: %s", std::strerror(errno));
        } else {
            // Vendor LVGL port configures the hifb layer once at startup and
            // then refreshes it with 0x4606 after each frame.
            if (ioctl(fbFd_, 0x4601u, &vinfo_) != 0) {
                LVFB_LOGW("hifb layer ioctl 0x4601 failed: %s", std::strerror(errno));
            } else {
                LVFB_LOGI("hifb layer configured (0x4601)");
            }
        }
    } else {
        LVFB_LOGW("cannot open %s for hifb refresh: %s", fbPath.c_str(), std::strerror(errno));
    }

    LVFB_LOGI("LVGL display initialized on %s", fbPath.c_str());
    return true;
}

void Display::commitFb()
{
    if (fbFd_ < 0) {
        return;
    }

    // HiFB does not scan out mmap writes until the layer is refreshed.
    // The vendor's own LVGL port uses this private ioctl after each frame.
    if (ioctl(fbFd_, 0x4606u, &vinfo_) != 0) {
        if (!warnedRefresh_) {
            LVFB_LOGW("hifb refresh ioctl 0x4606 failed: %s", std::strerror(errno));
            warnedRefresh_ = true;
        }
    }
}

void Display::shutdown()
{
    if (fbFd_ >= 0) {
        close(fbFd_);
        fbFd_ = -1;
    }
    if (lvInitialized_) {
        if (display_ != nullptr) {
            lv_display_delete(display_);
            display_ = nullptr;
        }
        lv_deinit();
        lvInitialized_ = false;
    }
}

void Display::refreshNow()
{
    if (display_ != nullptr) {
        lv_refr_now(display_);
    }
    commitFb();
}

uint32_t Display::timerHandler()
{
    if (!lvInitialized_) {
        return 16;
    }
    const uint32_t next = lv_timer_handler();
    commitFb();
    return next;
}

ConsoleMode::ConsoleMode() = default;

ConsoleMode::~ConsoleMode()
{
    restoreText();
}

void ConsoleMode::enterGraphics()
{
    if (ttyFd_ >= 0) {
        return;
    }
    ttyFd_ = open("/dev/tty0", O_RDWR | O_CLOEXEC);
    if (ttyFd_ < 0) {
        LVFB_LOGD("cannot open /dev/tty0: %s", std::strerror(errno));
        return;
    }
    if (ioctl(ttyFd_, KDSETMODE, KD_GRAPHICS) != 0) {
        LVFB_LOGW("KDSETMODE(KD_GRAPHICS) failed: %s", std::strerror(errno));
    } else {
        graphics_ = true;
        LVFB_LOGI("framebuffer console switched to graphics mode");
    }
}

void ConsoleMode::restoreText()
{
    if (ttyFd_ < 0) {
        return;
    }
    if (graphics_) {
        if (ioctl(ttyFd_, KDSETMODE, KD_TEXT) != 0) {
            LVFB_LOGW("KDSETMODE(KD_TEXT) failed: %s", std::strerror(errno));
        } else {
            LVFB_LOGI("framebuffer console restored to text mode");
        }
        graphics_ = false;
    }
    close(ttyFd_);
    ttyFd_ = -1;
}

} // namespace lvfb
