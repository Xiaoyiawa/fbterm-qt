#include "render/font.h"

#include "app/log.h"
#include "render/palette.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <sys/stat.h>
#include <unistd.h>

#include FT_SYNTHESIS_H

namespace lvfb {

std::vector<std::string> defaultFontPaths()
{
    return {
        "./unifont.otf",
        "./share/unifont.otf",
        "../share/unifont.otf",
        "/usr/local/share/lvfbterm/unifont.otf",
        "/usr/share/fonts/opentype/unifont/unifont.otf",
        "/usr/share/fonts/truetype/unifont/unifont.otf",
        "/usr/share/unifont/unifont.otf",
    };
}

FontManager::FontManager()
    : cache_(4096)
{
}

FontManager::~FontManager()
{
    for (size_t i = 0; i < faces_.size(); ++i) {
        if (faces_[i].face != nullptr) {
            FT_Done_Face(faces_[i].face);
        }
    }
    faces_.clear();
    if (library_ != nullptr) {
        FT_Done_FreeType(library_);
        library_ = nullptr;
    }
}

bool FontManager::loadFaces(const std::vector<std::string>& paths, std::string* error)
{
    std::vector<std::string> candidates = paths;
    if (candidates.empty()) {
        candidates = defaultFontPaths();
    }

    for (size_t i = 0; i < candidates.size(); ++i) {
        const std::string& path = candidates[i];
        struct stat st;
        if (stat(path.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            LVFB_LOGW("font not found: %s", path.c_str());
            continue;
        }

        FT_Face face = nullptr;
        const FT_Error err = FT_New_Face(library_, path.c_str(), 0, &face);
        if (err != 0 || face == nullptr) {
            LVFB_LOGW("cannot load font %s (FreeType error %d)", path.c_str(),
                      static_cast<int>(err));
            continue;
        }

        if (FT_Select_Charmap(face, FT_ENCODING_UNICODE) != 0) {
            LVFB_LOGW("font %s has no Unicode charmap", path.c_str());
            FT_Done_Face(face);
            continue;
        }

        Face f;
        f.face = face;
        f.path = path;
        faces_.push_back(f);
        LVFB_LOGI("font loaded: %s (%s)", path.c_str(), face->family_name != nullptr
                                                           ? face->family_name
                                                           : "unknown");
    }

    if (faces_.empty()) {
        if (error != nullptr) {
            *error = "no usable font found";
        }
        return false;
    }
    return true;
}

bool FontManager::isMonospace(FT_Face face, int cellWidth, int* firstMismatch,
                              int* firstAdvance) const
{
    if (firstMismatch != nullptr) {
        *firstMismatch = 0;
    }
    if (firstAdvance != nullptr) {
        *firstAdvance = 0;
    }

    for (uint32_t cp = 0x21; cp <= 0x7e; ++cp) {
        if (FT_Load_Char(face, cp, FT_LOAD_DEFAULT) != 0) {
            continue;
        }
        const int advance = static_cast<int>(face->glyph->advance.x >> 6);
        if (advance <= 0) {
            continue;
        }
        if (advance != cellWidth) {
            if (firstMismatch != nullptr) {
                *firstMismatch = static_cast<int>(cp);
            }
            if (firstAdvance != nullptr) {
                *firstAdvance = advance;
            }
            return false;
        }
    }
    return true;
}

bool FontManager::computeMetrics()
{
    if (faces_.empty()) {
        return false;
    }

    FT_Face primary = faces_[0].face;
    if (FT_Set_Pixel_Sizes(primary, 0, static_cast<FT_UInt>(metrics_.fontSize)) != 0) {
        return false;
    }

    const FT_Size_Metrics& sm = primary->size->metrics;
    const int ascender = static_cast<int>((sm.ascender + 63) >> 6);
    const int descender = static_cast<int>(sm.descender >> 6);

    int cellHeight = metrics_.fontSize;
    if (ascender - descender > cellHeight) {
        cellHeight = ascender - descender;
    }
    if (cellHeight < 1) {
        cellHeight = 1;
    }

    int cellWidth = 0;
    if (FT_Load_Char(primary, 'M', FT_LOAD_DEFAULT) == 0) {
        cellWidth = static_cast<int>(primary->glyph->advance.x >> 6);
    }
    if (cellWidth <= 0 && FT_Load_Char(primary, '0', FT_LOAD_DEFAULT) == 0) {
        cellWidth = static_cast<int>(primary->glyph->advance.x >> 6);
    }
    if (cellWidth <= 0) {
        cellWidth = static_cast<int>((sm.max_advance + 63) >> 6);
    }
    if (cellWidth <= 0) {
        cellWidth = metrics_.fontSize / 2;
    }
    if (cellWidth < 1) {
        cellWidth = 1;
    }

    metrics_.cellWidth = cellWidth;
    metrics_.cellHeight = cellHeight;
    metrics_.baseline = ascender;
    if (metrics_.baseline < 1) {
        metrics_.baseline = cellHeight * 3 / 4;
    }
    if (metrics_.baseline > cellHeight) {
        metrics_.baseline = cellHeight;
    }

    int mismatch = 0;
    int mismatchAdvance = 0;
    if (!isMonospace(primary, cellWidth, &mismatch, &mismatchAdvance)) {
        LVFB_LOGW("primary font is not monospace: U+%04X advance=%d, expected=%d; "
                  "continuing with cell width %d",
                  static_cast<unsigned>(mismatch), mismatchAdvance, cellWidth, cellWidth);
    } else {
        LVFB_LOGI("font metrics: %dx%d, baseline=%d, monospace=yes",
                  metrics_.cellWidth, metrics_.cellHeight, metrics_.baseline);
    }

    return true;
}

bool FontManager::applySize(std::string* error)
{
    if (faces_.empty()) {
        if (error != nullptr) {
            *error = "no font faces loaded";
        }
        return false;
    }

    for (size_t i = 0; i < faces_.size(); ++i) {
        if (FT_Set_Pixel_Sizes(faces_[i].face, 0,
                               static_cast<FT_UInt>(metrics_.fontSize)) != 0) {
            LVFB_LOGW("FT_Set_Pixel_Sizes failed for %s", faces_[i].path.c_str());
        }
    }

    if (!computeMetrics()) {
        if (error != nullptr) {
            *error = "cannot compute font metrics";
        }
        return false;
    }

    primaryPath_ = faces_[0].path;
    cache_.clear();
    return true;
}

bool FontManager::init(const std::vector<std::string>& paths, int fontSize,
                       std::string* error)
{
    if (fontSize < 8 || fontSize > 96) {
        if (error != nullptr) {
            *error = "font size out of range";
        }
        return false;
    }

    const FT_Error ftError = FT_Init_FreeType(&library_);
    if (ftError != 0) {
        if (error != nullptr) {
            *error = "FT_Init_FreeType failed";
        }
        return false;
    }

    metrics_.fontSize = fontSize;
    if (!loadFaces(paths, error)) {
        // Keep the terminal usable when the font is missing: use a tiny
        // synthetic cell so the caller can still show a login prompt and
        // report the problem. Characters are drawn as hollow boxes.
        builtin_ = true;
        const int scale = std::max(1, (fontSize + 8) / 16);
        metrics_.cellWidth = 8 * scale;
        metrics_.cellHeight = 16 * scale;
        metrics_.baseline = 13 * scale;
        primaryPath_.clear();
        LVFB_LOGW("no FreeType font available; falling back to box glyphs "
                  "(cell %dx%d)",
                  metrics_.cellWidth, metrics_.cellHeight);
        if (error != nullptr) {
            error->clear();
        }
        return true;
    }

    builtin_ = false;
    if (!applySize(error)) {
        return false;
    }

    LVFB_LOGI("font ready: %s, size=%d, cell=%dx%d", primaryPath_.c_str(),
              metrics_.fontSize, metrics_.cellWidth, metrics_.cellHeight);
    return true;
}

bool FontManager::setSize(int fontSize, std::string* error)
{
    if (fontSize < 8 || fontSize > 96) {
        if (error != nullptr) {
            *error = "font size out of range";
        }
        return false;
    }
    if (fontSize == metrics_.fontSize) {
        return true;
    }

    metrics_.fontSize = fontSize;
    if (builtin_) {
        const int scale = std::max(1, (fontSize + 8) / 16);
        metrics_.cellWidth = 8 * scale;
        metrics_.cellHeight = 16 * scale;
        metrics_.baseline = 13 * scale;
        cache_.clear();
        return true;
    }

    return applySize(error);
}

bool FontManager::renderGlyph(FT_Face face, uint32_t codepoint, bool bold, bool italic,
                              Glyph* out) const
{
    const FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
    if (glyphIndex == 0) {
        return false;
    }

    FT_Int32 flags = FT_LOAD_DEFAULT;
    const bool mono = (metrics_.fontSize % 16) == 0;
    flags |= mono ? FT_LOAD_TARGET_MONO : FT_LOAD_TARGET_NORMAL;

    if (FT_Load_Glyph(face, glyphIndex, flags) != 0) {
        return false;
    }

    FT_GlyphSlot slot = face->glyph;
    if (italic) {
        FT_GlyphSlot_Oblique(slot);
    }
    if (bold) {
        FT_GlyphSlot_Embolden(slot);
    }

    const FT_Render_Mode renderMode = mono ? FT_RENDER_MODE_MONO : FT_RENDER_MODE_NORMAL;
    if (FT_Render_Glyph(slot, renderMode) != 0) {
        return false;
    }

    const FT_Bitmap& bitmap = slot->bitmap;
    out->width = static_cast<int>(bitmap.width);
    out->height = static_cast<int>(bitmap.rows);
    out->left = slot->bitmap_left;
    out->top = slot->bitmap_top;
    out->advance = static_cast<int>(slot->advance.x >> 6);
    out->mono = mono;
    out->bitmap.clear();

    if (out->width <= 0 || out->height <= 0) {
        return true;
    }

    out->bitmap.resize(static_cast<size_t>(out->width) * static_cast<size_t>(out->height),
                       0);

    if (bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
        const int pitch = bitmap.pitch;
        for (int y = 0; y < out->height; ++y) {
            const unsigned char* row = bitmap.buffer + static_cast<long>(y) * pitch;
            for (int x = 0; x < out->width; ++x) {
                const int byte = row[x >> 3];
                const int bit = (byte >> (7 - (x & 7))) & 1;
                out->bitmap[static_cast<size_t>(y) * static_cast<size_t>(out->width) +
                            static_cast<size_t>(x)] = bit != 0 ? 255 : 0;
            }
        }
        return true;
    }

    if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
        const int pitch = bitmap.pitch;
        for (int y = 0; y < out->height; ++y) {
            const unsigned char* row = bitmap.buffer + static_cast<long>(y) * pitch;
            std::memcpy(&out->bitmap[static_cast<size_t>(y) * static_cast<size_t>(out->width)],
                        row, static_cast<size_t>(out->width));
        }
        return true;
    }

    return false;
}

bool FontManager::getGlyph(uint32_t codepoint, bool bold, bool italic,
                           const Glyph** out)
{
    if (out == nullptr) {
        return false;
    }
    *out = nullptr;
    if (builtin_ || faces_.empty()) {
        return false;
    }

    for (size_t faceIndex = 0; faceIndex < faces_.size(); ++faceIndex) {
        FT_Face face = faces_[faceIndex].face;
        const FT_UInt glyphIndex = FT_Get_Char_Index(face, codepoint);
        if (glyphIndex == 0) {
            continue;
        }

        const uint64_t key = (static_cast<uint64_t>(faceIndex) << 56) |
                             (static_cast<uint64_t>(glyphIndex) << 16) |
                             (bold ? 1u : 0u) << 1 | (italic ? 1u : 0u);
        const Glyph* cached = cache_.find(key);
        if (cached != nullptr) {
            *out = cached;
            return true;
        }

        Glyph glyph;
        if (!renderGlyph(face, codepoint, bold, italic, &glyph)) {
            continue;
        }
        const Glyph* stored = cache_.insert(key, std::move(glyph));
        if (stored != nullptr) {
            *out = stored;
            return true;
        }
    }

    return false;
}

} // namespace lvfb
