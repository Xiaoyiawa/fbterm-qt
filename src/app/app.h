#ifndef LVFB_APP_APP_H
#define LVFB_APP_APP_H

#include "app/options.h"
#include "input/input.h"
#include "render/font.h"
#include "render/renderer.h"
#include "term/pty.h"
#include "term/terminal.h"
#include "ui/canvas.h"
#include "ui/display.h"

#include <signal.h>
#include <sys/signalfd.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace lvfb {

class App {
public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run(const Options& options);

private:
    bool setupSignals();
    void closeSignals();
    void loop();

    void handleInputEvents(const std::vector<InputEvent>& events);
    void handleKey(int code, int value);
    void handleWheel(int delta);
    VTermModifier modifiers() const;
    int controlCode(uint32_t cp) const;

    void readPty();
    void handleChildExit();
    void handleSignals();
    void renderIfNeeded();
    void present();
    void applyFontSize(int fontSize);

    Options options_;
    ConsoleMode console_;
    Display display_;
    Canvas canvas_;
    FontManager font_;
    std::unique_ptr<Terminal> terminal_;
    std::unique_ptr<Pty> pty_;
    std::unique_ptr<Renderer> renderer_;
    InputManager input_;

    int signalFd_ = -1;
    sigset_t signalMask_;

    bool running_ = false;
    bool loginMode_ = false;
    bool childExitHandled_ = false;
    bool ptyEof_ = false;

    bool ctrl_ = false;
    bool alt_ = false;
    bool shift_ = false;
    bool caps_ = false;

    int64_t lastBlinkMs_ = 0;
};

} // namespace lvfb

#endif // LVFB_APP_APP_H
