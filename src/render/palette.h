#ifndef LVFB_RENDER_PALETTE_H
#define LVFB_RENDER_PALETTE_H

#include <cstdint>

namespace lvfb {

constexpr uint32_t kDefaultForeground = 0x00d0d0d0;
constexpr uint32_t kDefaultBackground = 0x00000000;
constexpr uint32_t kCursorColor = 0x00f0f0f0;

uint32_t paletteColor(int index);

} // namespace lvfb

#endif // LVFB_RENDER_PALETTE_H
