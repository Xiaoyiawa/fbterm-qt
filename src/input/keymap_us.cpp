#include "input/keymap_us.h"

#include <linux/input-event-codes.h>

namespace lvfb {

namespace {

void setPrintable(uint32_t codepoint, KeyMapping* out)
{
    out->printable = true;
    out->codepoint = codepoint;
}

void setSpecial(VTermKey key, KeyMapping* out)
{
    out->special = true;
    out->key = key;
}

void setModifier(KeyMapping* out)
{
    out->modifierOnly = true;
}

struct LetterKey {
    int code;
    char lower;
};

// Linux evdev keycodes are NOT alphabetical (KEY_A=30, KEY_B=48, ...).
// Keep the physical US QWERTY layout explicit.
const LetterKey kLetters[] = {
    {KEY_A, 'a'}, {KEY_B, 'b'}, {KEY_C, 'c'}, {KEY_D, 'd'}, {KEY_E, 'e'},
    {KEY_F, 'f'}, {KEY_G, 'g'}, {KEY_H, 'h'}, {KEY_I, 'i'}, {KEY_J, 'j'},
    {KEY_K, 'k'}, {KEY_L, 'l'}, {KEY_M, 'm'}, {KEY_N, 'n'}, {KEY_O, 'o'},
    {KEY_P, 'p'}, {KEY_Q, 'q'}, {KEY_R, 'r'}, {KEY_S, 's'}, {KEY_T, 't'},
    {KEY_U, 'u'}, {KEY_V, 'v'}, {KEY_W, 'w'}, {KEY_X, 'x'}, {KEY_Y, 'y'},
    {KEY_Z, 'z'},
};

bool lookupLetter(int code, bool shift, bool capsLock, KeyMapping* out)
{
    for (std::size_t i = 0; i < sizeof(kLetters) / sizeof(kLetters[0]); ++i) {
        if (kLetters[i].code != code) {
            continue;
        }
        uint32_t cp = static_cast<uint32_t>(kLetters[i].lower);
        if (shift != capsLock) {
            cp -= 32;
        }
        setPrintable(cp, out);
        return true;
    }
    return false;
}

} // namespace

bool mapKeyUS(int code, bool shift, bool capsLock, KeyMapping* out)
{
    if (out == nullptr) {
        return false;
    }
    *out = KeyMapping();

    if (lookupLetter(code, shift, capsLock, out)) {
        return true;
    }

    switch (code) {
    case KEY_1:
        setPrintable(shift ? '!' : '1', out);
        return true;
    case KEY_2:
        setPrintable(shift ? '@' : '2', out);
        return true;
    case KEY_3:
        setPrintable(shift ? '#' : '3', out);
        return true;
    case KEY_4:
        setPrintable(shift ? '$' : '4', out);
        return true;
    case KEY_5:
        setPrintable(shift ? '%' : '5', out);
        return true;
    case KEY_6:
        setPrintable(shift ? '^' : '6', out);
        return true;
    case KEY_7:
        setPrintable(shift ? '&' : '7', out);
        return true;
    case KEY_8:
        setPrintable(shift ? '*' : '8', out);
        return true;
    case KEY_9:
        setPrintable(shift ? '(' : '9', out);
        return true;
    case KEY_0:
        setPrintable(shift ? ')' : '0', out);
        return true;

    case KEY_GRAVE:
        setPrintable(shift ? '~' : '`', out);
        return true;
    case KEY_MINUS:
        setPrintable(shift ? '_' : '-', out);
        return true;
    case KEY_EQUAL:
        setPrintable(shift ? '+' : '=', out);
        return true;
    case KEY_LEFTBRACE:
        setPrintable(shift ? '{' : '[', out);
        return true;
    case KEY_RIGHTBRACE:
        setPrintable(shift ? '}' : ']', out);
        return true;
    case KEY_BACKSLASH:
    case KEY_102ND:
        setPrintable(shift ? '|' : '\\', out);
        return true;
    case KEY_SEMICOLON:
        setPrintable(shift ? ':' : ';', out);
        return true;
    case KEY_APOSTROPHE:
        setPrintable(shift ? '"' : '\'', out);
        return true;
    case KEY_COMMA:
        setPrintable(shift ? '<' : ',', out);
        return true;
    case KEY_DOT:
        setPrintable(shift ? '>' : '.', out);
        return true;
    case KEY_SLASH:
        setPrintable(shift ? '?' : '/', out);
        return true;
    case KEY_SPACE:
        setPrintable(' ', out);
        return true;

    case KEY_ENTER:
    case KEY_KPENTER:
        setSpecial(VTERM_KEY_ENTER, out);
        return true;
    case KEY_TAB:
        setSpecial(VTERM_KEY_TAB, out);
        return true;
    case KEY_BACKSPACE:
        setSpecial(VTERM_KEY_BACKSPACE, out);
        return true;
    case KEY_ESC:
        setSpecial(VTERM_KEY_ESCAPE, out);
        return true;

    case KEY_UP:
        setSpecial(VTERM_KEY_UP, out);
        return true;
    case KEY_DOWN:
        setSpecial(VTERM_KEY_DOWN, out);
        return true;
    case KEY_LEFT:
        setSpecial(VTERM_KEY_LEFT, out);
        return true;
    case KEY_RIGHT:
        setSpecial(VTERM_KEY_RIGHT, out);
        return true;
    case KEY_INSERT:
        setSpecial(VTERM_KEY_INS, out);
        return true;
    case KEY_DELETE:
        setSpecial(VTERM_KEY_DEL, out);
        return true;
    case KEY_HOME:
        setSpecial(VTERM_KEY_HOME, out);
        return true;
    case KEY_END:
        setSpecial(VTERM_KEY_END, out);
        return true;
    case KEY_PAGEUP:
        setSpecial(VTERM_KEY_PAGEUP, out);
        return true;
    case KEY_PAGEDOWN:
        setSpecial(VTERM_KEY_PAGEDOWN, out);
        return true;

    case KEY_F1:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(1)), out);
        return true;
    case KEY_F2:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(2)), out);
        return true;
    case KEY_F3:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(3)), out);
        return true;
    case KEY_F4:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(4)), out);
        return true;
    case KEY_F5:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(5)), out);
        return true;
    case KEY_F6:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(6)), out);
        return true;
    case KEY_F7:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(7)), out);
        return true;
    case KEY_F8:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(8)), out);
        return true;
    case KEY_F9:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(9)), out);
        return true;
    case KEY_F10:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(10)), out);
        return true;
    case KEY_F11:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(11)), out);
        return true;
    case KEY_F12:
        setSpecial(static_cast<VTermKey>(VTERM_KEY_FUNCTION(12)), out);
        return true;

    case KEY_KP0:
        setSpecial(VTERM_KEY_KP_0, out);
        return true;
    case KEY_KP1:
        setSpecial(VTERM_KEY_KP_1, out);
        return true;
    case KEY_KP2:
        setSpecial(VTERM_KEY_KP_2, out);
        return true;
    case KEY_KP3:
        setSpecial(VTERM_KEY_KP_3, out);
        return true;
    case KEY_KP4:
        setSpecial(VTERM_KEY_KP_4, out);
        return true;
    case KEY_KP5:
        setSpecial(VTERM_KEY_KP_5, out);
        return true;
    case KEY_KP6:
        setSpecial(VTERM_KEY_KP_6, out);
        return true;
    case KEY_KP7:
        setSpecial(VTERM_KEY_KP_7, out);
        return true;
    case KEY_KP8:
        setSpecial(VTERM_KEY_KP_8, out);
        return true;
    case KEY_KP9:
        setSpecial(VTERM_KEY_KP_9, out);
        return true;
    case KEY_KPSLASH:
        setSpecial(VTERM_KEY_KP_DIVIDE, out);
        return true;
    case KEY_KPASTERISK:
        setSpecial(VTERM_KEY_KP_MULT, out);
        return true;
    case KEY_KPMINUS:
        setSpecial(VTERM_KEY_KP_MINUS, out);
        return true;
    case KEY_KPPLUS:
        setSpecial(VTERM_KEY_KP_PLUS, out);
        return true;
    case KEY_KPDOT:
        setSpecial(VTERM_KEY_KP_PERIOD, out);
        return true;
    case KEY_KPCOMMA:
        setSpecial(VTERM_KEY_KP_COMMA, out);
        return true;
    case KEY_KPEQUAL:
        setSpecial(VTERM_KEY_KP_EQUAL, out);
        return true;

    case KEY_LEFTCTRL:
    case KEY_RIGHTCTRL:
    case KEY_LEFTSHIFT:
    case KEY_RIGHTSHIFT:
    case KEY_LEFTALT:
    case KEY_RIGHTALT:
    case KEY_LEFTMETA:
    case KEY_RIGHTMETA:
    case KEY_CAPSLOCK:
    case KEY_NUMLOCK:
        setModifier(out);
        return true;

    default:
        return false;
    }
}

} // namespace lvfb
