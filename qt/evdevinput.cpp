#include "evdevinput.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QKeyEvent>
#include <QSocketNotifier>
#include <QWheelEvent>
#include <qtermwidget.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace {

bool testBit(const unsigned long* bits, int bit)
{
    const int width = 8 * static_cast<int>(sizeof(unsigned long));
    return (bits[bit / width] & (1ul << (bit % width))) != 0;
}

struct LetterKey {
    int code;
    char lower;
};

const LetterKey kLetters[] = {
    {KEY_A, 'a'}, {KEY_B, 'b'}, {KEY_C, 'c'}, {KEY_D, 'd'}, {KEY_E, 'e'},
    {KEY_F, 'f'}, {KEY_G, 'g'}, {KEY_H, 'h'}, {KEY_I, 'i'}, {KEY_J, 'j'},
    {KEY_K, 'k'}, {KEY_L, 'l'}, {KEY_M, 'm'}, {KEY_N, 'n'}, {KEY_O, 'o'},
    {KEY_P, 'p'}, {KEY_Q, 'q'}, {KEY_R, 'r'}, {KEY_S, 's'}, {KEY_T, 't'},
    {KEY_U, 'u'}, {KEY_V, 'v'}, {KEY_W, 'w'}, {KEY_X, 'x'}, {KEY_Y, 'y'},
    {KEY_Z, 'z'},
};

bool mapPrintable(int code, bool shift, bool caps, int* key, QString* text)
{
    for (size_t i = 0; i < sizeof(kLetters) / sizeof(kLetters[0]); ++i) {
        if (kLetters[i].code != code) continue;
        const bool upper = (shift != caps);
        const char c = upper ? static_cast<char>(kLetters[i].lower - 32) : kLetters[i].lower;
        *key = Qt::Key_A + (kLetters[i].lower - 'a');
        *text = QString(QChar(c));
        return true;
    }

    char c = 0;
    switch (code) {
    case KEY_1: c = shift ? '!' : '1'; break;
    case KEY_2: c = shift ? '@' : '2'; break;
    case KEY_3: c = shift ? '#' : '3'; break;
    case KEY_4: c = shift ? '$' : '4'; break;
    case KEY_5: c = shift ? '%' : '5'; break;
    case KEY_6: c = shift ? '^' : '6'; break;
    case KEY_7: c = shift ? '&' : '7'; break;
    case KEY_8: c = shift ? '*' : '8'; break;
    case KEY_9: c = shift ? '(' : '9'; break;
    case KEY_0: c = shift ? ')' : '0'; break;
    case KEY_GRAVE: c = shift ? '~' : '`'; break;
    case KEY_MINUS: c = shift ? '_' : '-'; break;
    case KEY_EQUAL: c = shift ? '+' : '='; break;
    case KEY_LEFTBRACE: c = shift ? '{' : '['; break;
    case KEY_RIGHTBRACE: c = shift ? '}' : ']'; break;
    case KEY_BACKSLASH:
    case KEY_102ND: c = shift ? '|' : '\\'; break;
    case KEY_SEMICOLON: c = shift ? ':' : ';'; break;
    case KEY_APOSTROPHE: c = shift ? '"' : '\''; break;
    case KEY_COMMA: c = shift ? '<' : ','; break;
    case KEY_DOT: c = shift ? '>' : '.'; break;
    case KEY_SLASH: c = shift ? '?' : '/'; break;
    case KEY_SPACE: c = ' '; break;
    default: return false;
    }
    *key = static_cast<unsigned char>(c);
    *text = QString(QChar(static_cast<unsigned char>(c)));
    return true;
}

bool mapSpecial(int code, int* key)
{
    switch (code) {
    case KEY_ENTER:
    case KEY_KPENTER: *key = Qt::Key_Return; return true;
    case KEY_TAB: *key = Qt::Key_Tab; return true;
    case KEY_BACKSPACE: *key = Qt::Key_Backspace; return true;
    case KEY_ESC: *key = Qt::Key_Escape; return true;
    case KEY_UP: *key = Qt::Key_Up; return true;
    case KEY_DOWN: *key = Qt::Key_Down; return true;
    case KEY_LEFT: *key = Qt::Key_Left; return true;
    case KEY_RIGHT: *key = Qt::Key_Right; return true;
    case KEY_INSERT: *key = Qt::Key_Insert; return true;
    case KEY_DELETE: *key = Qt::Key_Delete; return true;
    case KEY_HOME: *key = Qt::Key_Home; return true;
    case KEY_END: *key = Qt::Key_End; return true;
    case KEY_PAGEUP: *key = Qt::Key_PageUp; return true;
    case KEY_PAGEDOWN: *key = Qt::Key_PageDown; return true;
    case KEY_F1: *key = Qt::Key_F1; return true;
    case KEY_F2: *key = Qt::Key_F2; return true;
    case KEY_F3: *key = Qt::Key_F3; return true;
    case KEY_F4: *key = Qt::Key_F4; return true;
    case KEY_F5: *key = Qt::Key_F5; return true;
    case KEY_F6: *key = Qt::Key_F6; return true;
    case KEY_F7: *key = Qt::Key_F7; return true;
    case KEY_F8: *key = Qt::Key_F8; return true;
    case KEY_F9: *key = Qt::Key_F9; return true;
    case KEY_F10: *key = Qt::Key_F10; return true;
    case KEY_F11: *key = Qt::Key_F11; return true;
    case KEY_F12: *key = Qt::Key_F12; return true;
    case KEY_KP0: *key = Qt::Key_0; return true;
    case KEY_KP1: *key = Qt::Key_1; return true;
    case KEY_KP2: *key = Qt::Key_2; return true;
    case KEY_KP3: *key = Qt::Key_3; return true;
    case KEY_KP4: *key = Qt::Key_4; return true;
    case KEY_KP5: *key = Qt::Key_5; return true;
    case KEY_KP6: *key = Qt::Key_6; return true;
    case KEY_KP7: *key = Qt::Key_7; return true;
    case KEY_KP8: *key = Qt::Key_8; return true;
    case KEY_KP9: *key = Qt::Key_9; return true;
    default: return false;
    }
}

} // namespace

EvdevInput::EvdevInput(QTermWidget* terminal, const QtOptions& options, QObject* parent)
    : QObject(parent)
    , terminal_(terminal)
    , options_(options)
{
    keyboardTimer_ = new QTimer(this);
    keyboardTimer_->setInterval(500);
    connect(keyboardTimer_, &QTimer::timeout, this, &EvdevInput::scanKeyboards);

    mouseTimer_ = new QTimer(this);
    mouseTimer_->setInterval(10000);
    connect(mouseTimer_, &QTimer::timeout, this, &EvdevInput::scanMice);

    if (!options_.keyboard.isEmpty()) openDevice(options_.keyboard, true);
    if (!options_.mouse.isEmpty()) openDevice(options_.mouse, false);

    scanKeyboards();
    scanMice();
    mouseTimer_->start();
}

EvdevInput::~EvdevInput()
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].fd >= 0) {
            ioctl(devices_[i].fd, EVIOCGRAB, 0);
            close(devices_[i].fd);
        }
    }
    devices_.clear();
}

bool EvdevInput::isOpen(const QString& path) const
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].path == path) return true;
    }
    return false;
}

bool EvdevInput::hasKeyboard() const
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].keyboard) return true;
    }
    return false;
}

bool EvdevInput::openDevice(const QString& path, bool wantKeyboard)
{
    if (isOpen(path)) return true;

    const int fd = open(path.toLocal8Bit().constData(),
                        O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;

    unsigned long evBits[(EV_MAX + 63) / 64] = {};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0) {
        close(fd);
        return false;
    }

    bool keyboard = false;
    bool mouse = false;
    if (wantKeyboard) {
        if (!testBit(evBits, EV_KEY)) { close(fd); return false; }
        unsigned long keyBits[(KEY_MAX + 63) / 64] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) < 0) {
            close(fd);
            return false;
        }
        keyboard = testBit(keyBits, KEY_A) && testBit(keyBits, KEY_ENTER) &&
                   testBit(keyBits, KEY_SPACE);
    } else {
        if (!testBit(evBits, EV_REL)) { close(fd); return false; }
        unsigned long relBits[(REL_MAX + 63) / 64] = {};
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relBits)), relBits) < 0) {
            close(fd);
            return false;
        }
        mouse = testBit(relBits, REL_WHEEL);
    }

    if (!keyboard && !mouse) {
        close(fd);
        return false;
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        qWarning() << "EVIOCGRAB failed for" << path << ":" << strerror(errno)
                   << "- closing to avoid duplicate input";
        close(fd);
        return false;
    }

    Device dev;
    dev.fd = fd;
    dev.keyboard = keyboard;
    dev.mouse = mouse;
    dev.path = path;

    QSocketNotifier* notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(notifier, &QSocketNotifier::activated, this,
            [this, fd](int) { readDevice(fd); });
    dev.notifier = notifier;
    devices_.push_back(dev);

    qInfo() << "evdev device added:" << path << (keyboard ? "keyboard" : "mouse");
    return true;
}

void EvdevInput::removeDevice(int fd)
{
    for (int i = 0; i < devices_.size(); ++i) {
        if (devices_[i].fd != fd) continue;
        const bool wasKeyboard = devices_[i].keyboard;
        qInfo() << "evdev device removed:" << devices_[i].path;
        if (devices_[i].notifier != nullptr) {
            devices_[i].notifier->setEnabled(false);
            devices_[i].notifier->deleteLater();
        }
        ioctl(fd, EVIOCGRAB, 0);
        close(fd);
        devices_.remove(i);
        if (wasKeyboard && !hasKeyboard() && !keyboardTimer_->isActive()) {
            keyboardTimer_->start();
        }
        return;
    }
}

void EvdevInput::scanKeyboards()
{
    if (hasKeyboard()) {
        keyboardTimer_->stop();
        return;
    }

    for (int i = 0; i < 64; ++i) {
        const QString path = QStringLiteral("/dev/input/event%1").arg(i);
        if (isOpen(path)) continue;
        if (openDevice(path, true)) {
            keyboardTimer_->stop();
            return;
        }
    }

    if (!keyboardTimer_->isActive()) keyboardTimer_->start();
}

void EvdevInput::scanMice()
{
    for (int i = 0; i < 64; ++i) {
        const QString path = QStringLiteral("/dev/input/event%1").arg(i);
        if (isOpen(path)) continue;
        openDevice(path, false);
    }
}

void EvdevInput::readDevice(int fd)
{
    bool removed = false;
    for (;;) {
        struct input_event evs[64];
        const ssize_t n = read(fd, evs, sizeof(evs));
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            removed = true;
            break;
        }
        if (n == 0) {
            removed = true;
            break;
        }

        const size_t count = static_cast<size_t>(n) / sizeof(struct input_event);
        for (size_t i = 0; i < count; ++i) {
            const struct input_event& ev = evs[i];
            if (ev.type == EV_KEY) {
                processKey(ev.code, ev.value);
            } else if (ev.type == EV_REL && ev.code == REL_WHEEL) {
                processWheel(ev.value);
            }
        }
    }
    if (removed) removeDevice(fd);
}

Qt::KeyboardModifiers EvdevInput::modifiers() const
{
    Qt::KeyboardModifiers mods;
    if (shift_) mods |= Qt::ShiftModifier;
    if (ctrl_) mods |= Qt::ControlModifier;
    if (alt_) mods |= Qt::AltModifier;
    return mods;
}

QWidget* EvdevInput::eventTarget() const
{
    QWidget* target = QApplication::focusWidget();
    if (target == nullptr && terminal_ != nullptr) {
        target = terminal_->findChild<QWidget*>();
    }
    if (target == nullptr) target = terminal_;
    return target;
}

void EvdevInput::sendKey(int key, Qt::KeyboardModifiers mods, const QString& text)
{
    if (imeFilter_ && imeFilter_(key, mods, text)) return;

    QWidget* target = eventTarget();
    if (target == nullptr) return;

    QKeyEvent event(QEvent::KeyPress, key, mods, text);
    QCoreApplication::sendEvent(target, &event);
}

void EvdevInput::processKey(int code, int value)
{
    switch (code) {
    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
        if (value == 1) { ctrl_ = true; shiftTapCandidate_ = false; }
        else if (value == 0) ctrl_ = false;
        return;
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
        if (value == 1) { alt_ = true; shiftTapCandidate_ = false; }
        else if (value == 0) alt_ = false;
        return;
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
        if (value == 1) {
            if (!shift_) { shift_ = true; shiftTapCandidate_ = true; }
        } else if (value == 0 && shift_) {
            shift_ = false;
            if (shiftTapCandidate_ && shiftTapHandler_) shiftTapHandler_();
            shiftTapCandidate_ = false;
        }
        return;
    case KEY_CAPSLOCK:
        if (value == 1) caps_ = !caps_;
        return;
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_NUMLOCK:
        return;
    default:
        break;
    }

    if (value == 0) return;
    if (value == 1) shiftTapCandidate_ = false;

    int key = 0;
    QString text;
    if (mapSpecial(code, &key)) {
        sendKey(key, modifiers(), QString());
        return;
    }
    if (mapPrintable(code, shift_, caps_, &key, &text)) {
        sendKey(key, modifiers(), text);
    }
}

void EvdevInput::processWheel(int delta)
{
    if (delta == 0 || terminal_ == nullptr) return;
    QWidget* target = eventTarget();
    if (target == nullptr) return;

    const QPoint center(terminal_->width() / 2, terminal_->height() / 2);
    QWheelEvent event(QPointF(center),
                      QPointF(terminal_->mapToGlobal(center)),
                      QPoint(),
                      QPoint(0, delta * 120),
                      Qt::NoButton,
                      modifiers(),
                      Qt::NoScrollPhase,
                      false);
    QCoreApplication::sendEvent(target, &event);
}
