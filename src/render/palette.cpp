#include "render/palette.h"

namespace lvfb {

uint32_t paletteColor(int index)
{
    static const uint32_t base[16] = {
        0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
        0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff,
    };

    if (index < 0) {
        index = 0;
    }
    if (index < 16) {
        return base[index];
    }
    if (index < 232) {
        static const int levels[6] = {0, 95, 135, 175, 215, 255};
        const int i = index - 16;
        const int r = levels[(i / 36) % 6];
        const int g = levels[(i / 6) % 6];
        const int b = levels[i % 6];
        return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) |
               static_cast<uint32_t>(b);
    }
    if (index < 256) {
        const int gray = 8 + (index - 232) * 10;
        return (static_cast<uint32_t>(gray) << 16) |
               (static_cast<uint32_t>(gray) << 8) | static_cast<uint32_t>(gray);
    }
    return 0xffffff;
}

} // namespace lvfb
