#ifndef LVFB_UI_CANVAS_H
#define LVFB_UI_CANVAS_H

#include <lvgl.h>

#include <cstdint>
#include <string>

namespace lvfb {

class Canvas {
public:
    Canvas();
    ~Canvas();

    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;

    bool create(int width, int height, std::string* error);
    void destroy();

    lv_obj_t* obj() const { return obj_; }
    int width() const { return width_; }
    int height() const { return height_; }
    int stridePixels() const { return stridePixels_; }
    uint32_t* pixels() const { return pixels_; }

    void fill(uint32_t color);
    void dropImageCache();
    void invalidate(int x, int y, int w, int h);
    void invalidateAll();
    bool writeToFb(const std::string& path);

private:
    lv_obj_t* obj_ = nullptr;
    lv_draw_buf_t* buf_ = nullptr;
    uint32_t* pixels_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    int stridePixels_ = 0;
};

} // namespace lvfb

#endif // LVFB_UI_CANVAS_H
