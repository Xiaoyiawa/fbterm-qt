#ifndef LVFB_INPUT_KEYMAP_US_H
#define LVFB_INPUT_KEYMAP_US_H

#include <cstdint>
#include <vterm_keycodes.h>

namespace lvfb {

struct KeyMapping {
    bool modifierOnly = false;
    bool special = false;
    bool printable = false;
    VTermKey key = VTERM_KEY_NONE;
    uint32_t codepoint = 0;
};

// Maps a Linux evdev keycode under a US QWERTY layout.
bool mapKeyUS(int code, bool shift, bool capsLock, KeyMapping* out);

} // namespace lvfb

#endif // LVFB_INPUT_KEYMAP_US_H
