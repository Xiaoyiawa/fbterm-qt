#include "app/app.h"

#include "app/log.h"
#include "input/keymap_us.h"
#include "render/palette.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

namespace lvfb {

namespace {

int64_t nowMs()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<int64_t>(ts.tv_sec) * 1000 +
           static_cast<int64_t>(ts.tv_nsec / 1000000);
}

bool isLoginCommand(const std::string& command)
{
    std::vector<std::string> args;
    std::string error;
    if (!splitCommand(command, &args, &error) || args.empty()) {
        return false;
    }
    const std::string& program = args[0];
    const std::string::size_type slash = program.find_last_of('/');
    const std::string base = slash == std::string::npos ? program : program.substr(slash + 1);
    return base == "login";
}

} // namespace

App::App()
{
    sigemptyset(&signalMask_);
}

App::~App()
{
    if (pty_ != nullptr) {
        if (terminal_ != nullptr) {
            terminal_->setPty(nullptr);
        }
        pty_->stop();
    }
    input_.stop();

    if (display_.lvDisplay() != nullptr && renderer_ != nullptr && terminal_ != nullptr) {
        canvas_.fill(kDefaultBackground);
        canvas_.invalidateAll();
        display_.refreshNow();
    }

    renderer_.reset();
    terminal_.reset();
    pty_.reset();
    canvas_.destroy();
    display_.shutdown();
    console_.restoreText();
    closeSignals();
}

int App::run(const Options& options)
{
    options_ = options;
    logSetLevel(options_.logLevel);
    loginMode_ = isLoginCommand(options_.shell);

    if (!setupSignals()) {
        return 1;
    }

    // Do NOT call KDSETMODE(KD_GRAPHICS) on hifb: on this vendor kernel it
    // leaves the graphics layer black even though fb0 contains valid pixels.
    // The kernel console is on ttyAMA0, so fbcon does not compete with us.

    std::string error;
    if (!display_.init(options_.fb, &error)) {
        LVFB_LOGE("%s", error.c_str());
        return 1;
    }

    lv_display_t* disp = display_.lvDisplay();
    const int width = lv_display_get_horizontal_resolution(disp);
    const int height = lv_display_get_vertical_resolution(disp);
    if (width <= 0 || height <= 0) {
        LVFB_LOGE("display reports invalid resolution %dx%d", width, height);
        return 1;
    }

    if (!canvas_.create(width, height, &error)) {
        LVFB_LOGE("%s", error.c_str());
        return 1;
    }

    if (!font_.init(options_.fontPaths, options_.fontSize, &error)) {
        LVFB_LOGE("%s", error.c_str());
        return 1;
    }

    const FontMetrics& metrics = font_.metrics();
    const int rows = std::max(1, height / std::max(1, metrics.cellHeight));
    const int cols = std::max(1, width / std::max(1, metrics.cellWidth));

    terminal_.reset(new Terminal(rows, cols, options_.scrollback));
    if (!terminal_->init(&error)) {
        LVFB_LOGE("%s", error.c_str());
        return 1;
    }

    renderer_.reset(new Renderer(canvas_, font_));

    pty_.reset(new Pty());
    if (!pty_->start(options_.shell, rows, cols, &error)) {
        LVFB_LOGE("%s", error.c_str());
        return 1;
    }
    terminal_->setPty(pty_.get());

    if (!input_.init(options_.keyboards, options_.mice, &error)) {
        LVFB_LOGE("%s", error.c_str());
    }

    LVFB_LOGI("terminal %dx%d, font %dpx, cell %dx%d, rpath-ready",
              cols, rows, metrics.fontSize, metrics.cellWidth, metrics.cellHeight);

    terminal_->markFull();
    renderer_->render(*terminal_);
    present();
    lastBlinkMs_ = nowMs();

    running_ = true;
    loop();

    return 0;
}

bool App::setupSignals()
{
    sigemptyset(&signalMask_);
    sigaddset(&signalMask_, SIGCHLD);
    sigaddset(&signalMask_, SIGTERM);
    sigaddset(&signalMask_, SIGINT);
    sigaddset(&signalMask_, SIGHUP);
    sigaddset(&signalMask_, SIGQUIT);
    sigaddset(&signalMask_, SIGPIPE);

    if (sigprocmask(SIG_BLOCK, &signalMask_, nullptr) != 0) {
        LVFB_LOGE("sigprocmask failed: %s", std::strerror(errno));
        return false;
    }

    signalFd_ = signalfd(-1, &signalMask_, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signalFd_ < 0) {
        LVFB_LOGE("signalfd failed: %s", std::strerror(errno));
        return false;
    }
    return true;
}

void App::closeSignals()
{
    if (signalFd_ >= 0) {
        close(signalFd_);
        signalFd_ = -1;
    }
    sigprocmask(SIG_UNBLOCK, &signalMask_, nullptr);
}

void App::handleSignals()
{
    for (;;) {
        struct signalfd_siginfo info;
        const ssize_t n = read(signalFd_, &info, sizeof(info));
        if (n != static_cast<ssize_t>(sizeof(info))) {
            break;
        }
        switch (info.ssi_signo) {
        case SIGCHLD:
            if (!ptyEof_) {
                readPty();
            }
            if (!childExitHandled_) {
                handleChildExit();
            }
            break;
        case SIGTERM:
        case SIGINT:
        case SIGHUP:
        case SIGQUIT:
            LVFB_LOGI("signal %u: exiting", info.ssi_signo);
            running_ = false;
            break;
        case SIGPIPE:
            break;
        default:
            break;
        }
    }
}

void App::readPty()
{
    if (pty_ == nullptr || pty_->masterFd() < 0 || ptyEof_) {
        return;
    }

    std::string data;
    bool eof = false;
    size_t total = 0;

    for (;;) {
        std::string chunk;
        if (!pty_->readSome(&chunk)) {
            eof = true;
            break;
        }
        if (chunk.empty()) {
            break;
        }
        data.append(chunk);
        total += chunk.size();
        if (total >= 256 * 1024) {
            break;
        }
    }

    if (!data.empty()) {
        terminal_->feed(data.data(), data.size());
    }
    terminal_->process();

    lastBlinkMs_ = nowMs();
    if (!data.empty()) {
        renderer_->wakeCursor(*terminal_);
    }

    if (eof) {
        ptyEof_ = true;
        handleChildExit();
    }

    renderIfNeeded();
    present();
}

void App::handleChildExit()
{
    if (childExitHandled_ || pty_ == nullptr) {
        return;
    }

    int status = 0;
    if (!pty_->reap(&status)) {
        return;
    }

    childExitHandled_ = true;
    ptyEof_ = true;
    if (WIFEXITED(status)) {
        LVFB_LOGW("pty child exited with status %d", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        LVFB_LOGW("pty child killed by signal %d", WTERMSIG(status));
    } else {
        LVFB_LOGW("pty child exited");
    }

    terminal_->setPty(nullptr);
    pty_->stop();

    if (!loginMode_) {
        running_ = false;
        return;
    }

    terminal_->reset();

    // fork() from a multithreaded process is only safe if the child is
    // single-threaded again. Stop the evdev reader around the restart.
    input_.stop();

    std::string error;
    if (!pty_->start(options_.shell, terminal_->rows(), terminal_->cols(), &error)) {
        LVFB_LOGE("cannot restart login: %s", error.c_str());
        running_ = false;
        return;
    }

    terminal_->setPty(pty_.get());
    input_.init(options_.keyboards, options_.mice, &error);
    childExitHandled_ = false;
    ptyEof_ = false;
    terminal_->markFull();
    LVFB_LOGI("login restarted");
}

void App::loop()
{
    while (running_) {
        const uint32_t next = display_.timerHandler();
        int timeout = next > 16 ? 16 : static_cast<int>(next);

        std::vector<pollfd> fds;
        const int ptyFd = pty_ != nullptr ? pty_->masterFd() : -1;

        if (pty_ != nullptr && pty_->running() && ptyFd >= 0 && !ptyEof_) {
            struct pollfd pfd;
            pfd.fd = ptyFd;
            pfd.events = POLLIN;
            if (pty_->hasPendingWrites()) {
                pfd.events = static_cast<short>(pfd.events | POLLOUT);
            }
            pfd.revents = 0;
            fds.push_back(pfd);
        }

        if (signalFd_ >= 0) {
            struct pollfd pfd;
            pfd.fd = signalFd_;
            pfd.events = POLLIN;
            pfd.revents = 0;
            fds.push_back(pfd);
        }

        const int ret = poll(fds.data(), static_cast<nfds_t>(fds.size()), timeout);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LVFB_LOGE("poll failed: %s", std::strerror(errno));
            break;
        }

        if (ret > 0) {
            for (size_t i = 0; i < fds.size(); ++i) {
                const short revents = fds[i].revents;
                if (revents == 0) {
                    continue;
                }
                if (ptyFd >= 0 && fds[i].fd == ptyFd) {
                    if ((revents & POLLOUT) != 0 && pty_ != nullptr) {
                        if (!pty_->flushWrites()) {
                            ptyEof_ = true;
                        }
                    }
                    if ((revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                        readPty();
                    }
                } else if (fds[i].fd == signalFd_) {
                    handleSignals();
                }
            }
        }

        std::vector<InputEvent> events;
        input_.drainEvents(&events);
        if (!events.empty()) {
            handleInputEvents(events);
        }

        if (!running_) {
            break;
        }

        renderIfNeeded();

        const int64_t now = nowMs();
        if (terminal_ != nullptr && terminal_->cursorVisible() &&
            terminal_->cursorBlink() && terminal_->scrollOffset() == 0) {
            if (lastBlinkMs_ == 0) {
                lastBlinkMs_ = now;
            } else if (now - lastBlinkMs_ >= 500) {
                renderer_->toggleCursor(*terminal_);
                present();
                lastBlinkMs_ = now;
            }
        } else {
            lastBlinkMs_ = now;
        }
    }
}

void App::present()
{
    // LVGL flushes its own draw buffer; then push the whole canvas through
    // hifb's sequential write() path, which is the only path this driver
    // actually scans out.
    display_.refreshNow();
}

void App::renderIfNeeded()
{
    if (terminal_ != nullptr && renderer_ != nullptr && terminal_->hasDamage()) {
        renderer_->render(*terminal_);
        present();
    }
}

VTermModifier App::modifiers() const
{
    int mod = 0;
    if (shift_) {
        mod |= VTERM_MOD_SHIFT;
    }
    if (alt_) {
        mod |= VTERM_MOD_ALT;
    }
    if (ctrl_) {
        mod |= VTERM_MOD_CTRL;
    }
    return static_cast<VTermModifier>(mod);
}

int App::controlCode(uint32_t cp) const
{
    if (cp >= 'a' && cp <= 'z') {
        return static_cast<int>(cp - 'a' + 1);
    }
    if (cp >= 'A' && cp <= 'Z') {
        return static_cast<int>(cp - 'A' + 1);
    }
    switch (cp) {
    case ' ':
    case '@':
    case '2':
        return 0;
    case '[':
        return 0x1b;
    case '\\':
        return 0x1c;
    case ']':
        return 0x1d;
    case '^':
    case '6':
        return 0x1e;
    case '_':
    case '-':
    case '/':
        return 0x1f;
    case '?':
        return 0x7f;
    default:
        return -1;
    }
}

void App::handleKey(int code, int value)
{
    const bool isPress = value == 1;
    const bool isRelease = value == 0;

    switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        if (isPress) {
            ctrl_ = true;
        } else if (isRelease) {
            ctrl_ = false;
        }
        return;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        if (isPress) {
            alt_ = true;
        } else if (isRelease) {
            alt_ = false;
        }
        return;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        if (isPress) {
            shift_ = true;
        } else if (isRelease) {
            shift_ = false;
        }
        return;
    case KEY_CAPSLOCK:
        if (isPress) {
            caps_ = !caps_;
        }
        return;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_NUMLOCK:
        return;
    default:
        break;
    }

    if (isRelease) {
        return;
    }

    if (ctrl_ && alt_ && code == KEY_Q) {
        LVFB_LOGI("Ctrl+Alt+Q: exiting");
        running_ = false;
        return;
    }

    KeyMapping mapping;
    if (!mapKeyUS(code, shift_, caps_, &mapping) || mapping.modifierOnly) {
        return;
    }

    if (mapping.special) {
        terminal_->sendKey(mapping.key, modifiers());
        return;
    }

    if (!mapping.printable) {
        return;
    }

    if (ctrl_) {
        const int cc = controlCode(mapping.codepoint);
        if (cc >= 0) {
            char buffer[2];
            size_t length = 0;
            if (alt_) {
                buffer[length++] = 0x1b;
            }
            buffer[length++] = static_cast<char>(cc);
            terminal_->writeRaw(buffer, length);
        } else {
            terminal_->sendUnichar(mapping.codepoint, modifiers());
        }
        return;
    }

    const VTermModifier mod = alt_ ? VTERM_MOD_ALT : VTERM_MOD_NONE;
    terminal_->sendUnichar(mapping.codepoint, mod);
}

void App::handleWheel(int delta)
{
    if (delta == 0 || terminal_ == nullptr) {
        return;
    }

    const int direction = delta > 0 ? 1 : -1;
    int steps = delta > 0 ? delta : -delta;
    if (steps > 10) {
        steps = 10;
    }

    if (terminal_->mouseMode() != VTERM_PROP_MOUSE_NONE) {
        for (int i = 0; i < steps; ++i) {
            terminal_->sendMouseWheel(direction, modifiers());
        }
        return;
    }

    if (ctrl_) {
        applyFontSize(font_.fontSize() + direction * 2 * steps);
        return;
    }

    if (terminal_->altScreen()) {
        for (int i = 0; i < steps; ++i) {
            terminal_->sendKey(direction > 0 ? VTERM_KEY_UP : VTERM_KEY_DOWN,
                               modifiers());
        }
        return;
    }

    const int before = terminal_->scrollOffset();
    terminal_->scrollBy(direction * 3 * steps);
    if (terminal_->scrollOffset() != before) {
        lastBlinkMs_ = nowMs();
    }
}

void App::handleInputEvents(const std::vector<InputEvent>& events)
{
    for (size_t i = 0; i < events.size(); ++i) {
        const InputEvent& ev = events[i];
        if (ev.type == InputEventType::Key) {
            handleKey(ev.code, ev.value);
        } else {
            handleWheel(ev.wheel);
        }
        if (!running_) {
            break;
        }
    }

    if (terminal_ != nullptr) {
        lastBlinkMs_ = nowMs();
        renderer_->wakeCursor(*terminal_);
        renderIfNeeded();
        present();
    }
}

void App::applyFontSize(int fontSize)
{
    if (terminal_ == nullptr || renderer_ == nullptr) {
        return;
    }
    if (fontSize < 8) {
        fontSize = 8;
    }
    if (fontSize > 96) {
        fontSize = 96;
    }
    if (fontSize == font_.fontSize()) {
        return;
    }

    const int oldSize = font_.fontSize();
    std::string error;
    if (!font_.setSize(fontSize, &error)) {
        LVFB_LOGW("cannot change font size to %d: %s", fontSize, error.c_str());
        if (!font_.setSize(oldSize, &error)) {
            LVFB_LOGE("cannot restore font size %d: %s", oldSize, error.c_str());
            running_ = false;
        }
        return;
    }

    const int rows = renderer_->rowsForHeight(canvas_.height());
    const int cols = renderer_->colsForWidth(canvas_.width());
    if (rows != terminal_->rows() || cols != terminal_->cols()) {
        terminal_->resize(rows, cols);
    } else {
        terminal_->markFull();
    }

    renderer_->onFontChanged();
    renderer_->render(*terminal_);
    present();
    lastBlinkMs_ = nowMs();

    LVFB_LOGI("font size -> %d, grid %dx%d", font_.fontSize(), cols, rows);
}

} // namespace lvfb
