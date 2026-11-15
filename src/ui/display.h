#ifndef LVFB_UI_DISPLAY_H
#define LVFB_UI_DISPLAY_H

#include <lvgl.h>

#include <cstdint>
#include <linux/fb.h>
#include <string>

namespace lvfb {

class Display {
public:
    Display();
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    bool init(const std::string& fbPath, std::string* error);
    void shutdown();

    lv_display_t* lvDisplay() const { return display_; }
    uint32_t timerHandler();
    void refreshNow();
    void commitFb();

private:
    lv_display_t* display_ = nullptr;
    bool lvInitialized_ = false;
    int fbFd_ = -1;
    struct fb_var_screeninfo vinfo_;
    bool warnedRefresh_ = false;
};

// Stops the Linux framebuffer console from drawing over our canvas while the
// terminal is running. Restores KD_TEXT on destruction.
class ConsoleMode {
public:
    ConsoleMode();
    ~ConsoleMode();

    void enterGraphics();
    void restoreText();

private:
    int ttyFd_ = -1;
    bool graphics_ = false;
};

} // namespace lvfb

#endif // LVFB_UI_DISPLAY_H
