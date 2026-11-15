/**
 * Minimal lv_conf.h for lvfbterm.
 *
 * We use LVGL only as:
 *   - the /dev/fb0 Linux framebuffer display driver,
 *   - a full-screen lv_canvas that we render into ourselves,
 *   - a tick/timer source and partial-area flush.
 *
 * All other widgets/themes are left at LVGL defaults.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

/* Use the C library for allocation/strings: the process is Linux, not a
 * bare-metal target, and the canvas alone is 1920*1080*4 bytes. */
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB

/* The framebuffer is XRGB8888 (32 bpp, B/G/R/X in memory on little-endian). */
#define LV_COLOR_FORMAT_DEFAULT LV_COLOR_FORMAT_XRGB8888

/* Logging goes to stderr; when run from SSH this is visible immediately. */
#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1
#define LV_LOG_USE_TIMESTAMP 0
#define LV_LOG_USE_FILE_LINE 1

/* 60 Hz upper bound; the terminal's own damage tracking decides how much is
 * actually rendered. */
#define LV_DEF_REFR_PERIOD 16

/* Linux fbdev driver. */
#define LV_USE_LINUX_FBDEV 1
#define LV_LINUX_FBDEV_BSD 0
#define LV_LINUX_FBDEV_RENDER_MODE LV_DISPLAY_RENDER_MODE_PARTIAL
#define LV_LINUX_FBDEV_BUFFER_COUNT 2
#define LV_LINUX_FBDEV_BUFFER_SIZE 40
#define LV_LINUX_FBDEV_MMAP 1
#define LV_LINUX_FBDEV_VSYNC 0

/* Canvas is our only widget. */
#define LV_USE_CANVAS 1

/* Built-in monospace font, used only when no FreeType font can be loaded. */
#define LV_FONT_UNSCII_16 1
#define LV_FONT_DEFAULT LV_FONT_DEFAULT_UNSCII_16

/* Not needed by the terminal. Keep binary smaller. */
#define LV_USE_FREETYPE 0
#define LV_USE_TINY_TTF 0
#define LV_USE_IMGFONT 0
#define LV_USE_SDL 0
#define LV_USE_DRM 0
#define LV_USE_OPENGLES 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_SYSMON 0

#endif /* LV_CONF_H */
