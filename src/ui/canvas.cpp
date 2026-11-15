#include "ui/canvas.h"

#include "app/log.h"

#include <algorithm>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace lvfb {

Canvas::Canvas() = default;

Canvas::~Canvas()
{
    destroy();
}

bool Canvas::create(int width, int height, std::string* error)
{
    destroy();

    if (width <= 0 || height <= 0) {
        if (error != nullptr) {
            *error = "invalid canvas size";
        }
        return false;
    }

    buf_ = lv_draw_buf_create(static_cast<uint32_t>(width), static_cast<uint32_t>(height),
                              LV_COLOR_FORMAT_XRGB8888,
                              static_cast<uint32_t>(width) * 4u);
    if (buf_ == nullptr) {
        if (error != nullptr) {
            *error = "lv_draw_buf_create failed";
        }
        return false;
    }

    obj_ = lv_canvas_create(lv_screen_active());
    if (obj_ == nullptr) {
        lv_draw_buf_destroy(buf_);
        buf_ = nullptr;
        if (error != nullptr) {
            *error = "lv_canvas_create failed";
        }
        return false;
    }

    lv_canvas_set_draw_buf(obj_, buf_);
    lv_obj_set_pos(obj_, 0, 0);
    lv_obj_set_size(obj_, width, height);
    lv_obj_set_scrollable(obj_, false);
    lv_obj_set_clickable(obj_, false);
    lv_obj_set_style_pad_all(obj_, 0, 0);
    lv_obj_set_style_border_width(obj_, 0, 0);
    lv_obj_set_style_radius(obj_, 0, 0);

    width_ = width;
    height_ = height;
    stridePixels_ = static_cast<int>(buf_->header.stride / 4u);
    pixels_ = reinterpret_cast<uint32_t*>(buf_->data);

    std::memset(pixels_, 0, static_cast<size_t>(stridePixels_) * height_ * sizeof(uint32_t));
    invalidateAll();

    LVFB_LOGI("canvas created: %dx%d stride=%d", width_, height_, stridePixels_);
    return true;
}

void Canvas::destroy()
{
    if (obj_ != nullptr) {
        lv_obj_delete(obj_);
        obj_ = nullptr;
    }
    if (buf_ != nullptr) {
        lv_draw_buf_destroy(buf_);
        buf_ = nullptr;
    }
    pixels_ = nullptr;
    width_ = 0;
    height_ = 0;
    stridePixels_ = 0;
}

void Canvas::fill(uint32_t color)
{
    if (pixels_ == nullptr) {
        return;
    }
    for (int y = 0; y < height_; ++y) {
        uint32_t* row = pixels_ + static_cast<size_t>(y) * stridePixels_;
        for (int x = 0; x < width_; ++x) {
            row[x] = color;
        }
    }
}

void Canvas::dropImageCache()
{
    // We draw directly into the draw buffer backing the canvas image. LVGL
    // caches decoded image data keyed by the source pointer, so the cache
    // entry must be dropped before the changed pixels can be seen.
    if (buf_ != nullptr) {
        lv_image_cache_drop(buf_);
    }
}

void Canvas::invalidate(int x, int y, int w, int h)
{
    if (obj_ == nullptr || buf_ == nullptr || w <= 0 || h <= 0) {
        return;
    }
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(width_, x + w);
    const int y1 = std::min(height_, y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    lv_area_t area;
    area.x1 = x0;
    area.y1 = y0;
    area.x2 = x1 - 1;
    area.y2 = y1 - 1;
    lv_obj_invalidate_area(obj_, &area);
}

void Canvas::invalidateAll()
{
    dropImageCache();
    invalidate(0, 0, width_, height_);
}

bool Canvas::writeToFb(const std::string& path)
{
    if (pixels_ == nullptr || width_ <= 0 || height_ <= 0) {
        return false;
    }

    // Match the vendor test program: open the fb, write sequentially from
    // offset 0, close. No mmap, no pwrite, no lseek.
    const int fd = open(path.c_str(), O_WRONLY | O_CLOEXEC);
    if (fd < 0) {
        LVFB_LOGW("cannot open %s for full-frame write: %s", path.c_str(),
                  std::strerror(errno));
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(pixels_);
    size_t total = static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4u;
    // hifb's write() path behaves best with small chunks, like the vendor
    // test program (256 bytes) / ffmpeg. A single 8 MB write can be truncated.
    size_t done = 0;
    while (done < total) {
        size_t chunk = total - done;
        if (chunk > 4096) {
            chunk = 4096;
        }
        const ssize_t n = write(fd, data + done, chunk);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            LVFB_LOGW("full-frame write failed at %zu/%zu: %s", done, total,
                      std::strerror(errno));
            close(fd);
            return false;
        }
        if (n == 0) {
            LVFB_LOGW("full-frame write stalled at %zu/%zu", done, total);
            break;
        }
        done += static_cast<size_t>(n);
    }
    close(fd);
    if (done != total) {
        LVFB_LOGW("full-frame write incomplete: %zu/%zu", done, total);
        return false;
    }
    return true;
}

} // namespace lvfb
