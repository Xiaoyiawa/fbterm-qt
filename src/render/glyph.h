#ifndef LVFB_RENDER_GLYPH_H
#define LVFB_RENDER_GLYPH_H

#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <vector>

namespace lvfb {

struct Glyph {
    int width = 0;
    int height = 0;
    int left = 0;  // bitmap_left
    int top = 0;   // bitmap_top
    int advance = 0;
    bool mono = false;
    std::vector<uint8_t> bitmap; // width * height, 0..255
};

class GlyphCache {
public:
    explicit GlyphCache(size_t capacity);

    const Glyph* find(uint64_t key);
    const Glyph* insert(uint64_t key, Glyph&& glyph);
    void clear();
    size_t size() const { return map_.size(); }

private:
    struct Entry {
        Glyph glyph;
        std::list<uint64_t>::iterator lru;
    };

    std::unordered_map<uint64_t, Entry> map_;
    std::list<uint64_t> lru_;
    size_t capacity_;
};

} // namespace lvfb

#endif // LVFB_RENDER_GLYPH_H
