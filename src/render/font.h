#ifndef LVFB_RENDER_FONT_H
#define LVFB_RENDER_FONT_H

#include "render/glyph.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <string>
#include <vector>

namespace lvfb {

struct FontMetrics {
    int fontSize = 0;
    int cellWidth = 0;
    int cellHeight = 0;
    int baseline = 0;
};

class FontManager {
public:
    FontManager();
    ~FontManager();

    FontManager(const FontManager&) = delete;
    FontManager& operator=(const FontManager&) = delete;

    bool init(const std::vector<std::string>& paths, int fontSize, std::string* error);
    bool setSize(int fontSize, std::string* error);
    int fontSize() const { return metrics_.fontSize; }
    const FontMetrics& metrics() const { return metrics_; }
    const std::string& primaryPath() const { return primaryPath_; }
    bool builtin() const { return builtin_; }

    // Returns false when no face has this codepoint. |out| points into the
    // internal cache and stays valid until the next clear()/setSize().
    bool getGlyph(uint32_t codepoint, bool bold, bool italic, const Glyph** out);

private:
    struct Face {
        FT_Face face = nullptr;
        std::string path;
    };

    bool loadFaces(const std::vector<std::string>& paths, std::string* error);
    bool applySize(std::string* error);
    bool computeMetrics();
    bool isMonospace(FT_Face face, int cellWidth, int* firstMismatch,
                     int* firstAdvance) const;
    bool renderGlyph(FT_Face face, uint32_t codepoint, bool bold, bool italic,
                     Glyph* out) const;

    FT_Library library_ = nullptr;
    std::vector<Face> faces_;
    FontMetrics metrics_;
    GlyphCache cache_;
    bool builtin_ = false;
    std::string primaryPath_;
};

std::vector<std::string> defaultFontPaths();

} // namespace lvfb

#endif // LVFB_RENDER_FONT_H
