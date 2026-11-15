#ifndef LVFB_RENDER_RENDERER_H
#define LVFB_RENDER_RENDERER_H

#include "render/font.h"
#include "term/terminal.h"
#include "ui/canvas.h"

#include <cstdint>
#include <vector>

namespace lvfb {

class Renderer {
public:
    Renderer(Canvas& canvas, FontManager& font);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void render(Terminal& terminal);
    void renderAll(Terminal& terminal);
    void toggleCursor(Terminal& terminal);
    void wakeCursor(Terminal& terminal);
    void onFontChanged();

    const FontMetrics& metrics() const { return font_.metrics(); }
    int rowsForHeight(int height) const;
    int colsForWidth(int width) const;

private:
    struct Rect {
        int x = 0;
        int y = 0;
        int w = 0;
        int h = 0;
    };

    void drawAllRows(Terminal& terminal);
    void drawDirtyRows(Terminal& terminal);
    void drawRow(Terminal& terminal, int row, int minCol, int maxCol);
    void drawCell(Terminal& terminal, int row, int col, const VTermScreenCell& cell);
    void drawGlyphRun(Terminal& terminal, const VTermScreenCell& cell, int x, int y,
                      int cellPixelWidth, uint32_t fg);
    void drawHollowBox(int x, int y, int w, int h, uint32_t color);

    bool applyMoves(Terminal& terminal);
    bool movePixels(Terminal& terminal, const MoveRect& move);

    void fillRect(int x, int y, int w, int h, uint32_t color);
    void blendGlyph(int x, int y, const Glyph& glyph, uint32_t color, int clipX,
                    int clipY, int clipW, int clipH);
    void invertRect(int x, int y, int w, int h);

    void drawCursor(Terminal& terminal);
    void eraseCursor();
    void saveCursorPatch(int x, int y, int w, int h);
    void addInvalid(int x, int y, int w, int h);
    void addInvalidAll();
    void flushInvalid();

    void updateOrigin(const Terminal& terminal);

    Canvas& canvas_;
    FontManager& font_;

    int originX_ = 0;
    int originY_ = 0;

    std::vector<Rect> invalid_;
    bool invalidAll_ = false;

    std::vector<uint32_t> cursorPatch_;
    int cursorPatchX_ = 0;
    int cursorPatchY_ = 0;
    int cursorPatchW_ = 0;
    int cursorPatchH_ = 0;
    bool cursorOn_ = true;
};

} // namespace lvfb

#endif // LVFB_RENDER_RENDERER_H
