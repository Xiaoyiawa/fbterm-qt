#include "render/glyph.h"

#include <utility>

namespace lvfb {

GlyphCache::GlyphCache(size_t capacity)
    : capacity_(capacity)
{
}

const Glyph* GlyphCache::find(uint64_t key)
{
    auto it = map_.find(key);
    if (it == map_.end()) {
        return nullptr;
    }
    lru_.splice(lru_.begin(), lru_, it->second.lru);
    return &it->second.glyph;
}

const Glyph* GlyphCache::insert(uint64_t key, Glyph&& glyph)
{
    auto found = map_.find(key);
    if (found != map_.end()) {
        return &found->second.glyph;
    }
    if (capacity_ == 0) {
        return nullptr;
    }

    while (map_.size() >= capacity_ && !lru_.empty()) {
        const uint64_t victim = lru_.back();
        lru_.pop_back();
        map_.erase(victim);
    }

    lru_.push_front(key);
    Entry entry;
    entry.glyph = std::move(glyph);
    entry.lru = lru_.begin();

    auto inserted = map_.emplace(key, std::move(entry));
    return &inserted.first->second.glyph;
}

void GlyphCache::clear()
{
    map_.clear();
    lru_.clear();
}

} // namespace lvfb
