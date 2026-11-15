#include "render/renderer.h"

#include "app/log.h"
#include "render/palette.h"

#include <algorithm>
#include <cstring>

namespace lvfb {

namespace {

inline uint32_t blendPixel(uint32_t dst, uint32_t src, uint8_t alpha)
{
    if (alpha == 0) {
        return dst;
    }
    if (alpha == 255) {
        return src;
    }
    const uint32_t ia = 255u - alpha;
    const uint32_t dr = (dst >> 16) & 0xffu;
    const uint32_t dg = (dst >> 8) & 0xffu;
    const uint32_t db = dst & 0xffu;
    const uint32_t sr = (src >> 16) & 0xffu;
    const uint32_t sg = (src >> 8) & 0xffu;
    const uint32_t sb = src & 0xffu;
    const uint32_t r = (dr * ia + sr * alpha + 127u) / 255u;
    const uint32_t g = (dg * ia + sg * alpha + 127u) / 255u;
    const uint32_t b = (db * ia + sb * alpha + 127u) / 255u;
    return (r << 16) | (g << 8) | b;
}

} // namespace

Renderer::Renderer(Canvas& canvas, FontManager& font)
    : canvas_(canvas)
    , font_(font)
{
}

Renderer::~Renderer() = default;

int Renderer::rowsForHeight(int height) const
{
    const int cellH = std::max(1, font_.metrics().cellHeight);
    return std::max(1, height / cellH);
}

int Renderer::colsForWidth(int width) const
{
    const int cellW = std::max(1, font_.metrics().cellWidth);
    return std::max(1, width / cellW);
}

void Renderer::onFontChanged()
{
    cursorPatch_.clear();
    cursorOn_ = true;
    invalid_.clear();
    invalidAll_ = false;
}

void Renderer::updateOrigin(const Terminal& terminal)
{
    const FontMetrics& m = font_.metrics();
    const int gridW = terminal.cols() * std::max(1, m.cellWidth);
    const int gridH = terminal.rows() * std::max(1, m.cellHeight);
    originX_ = std::max(0, (canvas_.width() - gridW) / 2);
    originY_ = std::max(0, (canvas_.height() - gridH) / 2);
}

void Renderer::renderAll(Terminal& terminal)
{
    terminal.markFull();
    render(terminal);
}

void Renderer::render(Terminal& terminal)
{
    invalid_.clear();
    invalidAll_ = false;

    updateOrigin(terminal);

    if (!terminal.cursorBlink()) {
        cursorOn_ = true;
    }

    eraseCursor();

    if (terminal.fullRedraw()) {
        canvas_.fill(kDefaultBackground);
        drawAllRows(terminal);
        addInvalidAll();
    } else if (!applyMoves(terminal)) {
        canvas_.fill(kDefaultBackground);
        drawAllRows(terminal);
        addInvalidAll();
    } else {
        drawDirtyRows(terminal);
    }

    drawCursor(terminal);
    flushInvalid();

    terminal.clearDamage();
}

void Renderer::drawAllRows(Terminal& terminal)
{
    for (int row = 0; row < terminal.rows(); ++row) {
        drawRow(terminal, row, 0, terminal.cols() - 1);
    }
}

void Renderer::drawDirtyRows(Terminal& terminal)
{
    for (int row = 0; row < terminal.rows(); ++row) {
        const int minCol = terminal.dirtyMinCol(row);
        const int maxCol = terminal.dirtyMaxCol(row);
        if (minCol < 0 || maxCol < 0) {
            continue;
        }
        drawRow(terminal, row, minCol, maxCol);
    }
}

void Renderer::drawRow(Terminal& terminal, int row, int minCol, int maxCol)
{
    const FontMetrics& m = font_.metrics();
    const int cellW = std::max(1, m.cellWidth);
    const int cellH = std::max(1, m.cellHeight);
    if (row < 0 || row >= terminal.rows()) {
        return;
    }

    minCol = std::max(0, minCol - 1);
    maxCol = std::min(terminal.cols() - 1, maxCol + 1);
    if (minCol > maxCol) {
        return;
    }

    for (int col = minCol; col <= maxCol; ++col) {
        VTermScreenCell cell;
        if (!terminal.getViewCell(row, col, &cell)) {
            cell = blankCell();
        }
        if (cell.width == 0) {
            // Continuation of a wide glyph. The first cell paints both halves.
            continue;
        }
        drawCell(terminal, row, col, cell);
        if (cell.width > 1) {
            col += cell.width - 1;
        }
    }

    addInvalid(originX_ + minCol * cellW, originY_ + row * cellH,
               (maxCol - minCol + 1) * cellW, cellH);
}

void Renderer::drawCell(Terminal& terminal, int row, int col,
                        const VTermScreenCell& cell)
{
    const FontMetrics& m = font_.metrics();
    const int cellW = std::max(1, m.cellWidth);
    const int cellH = std::max(1, m.cellHeight);
    const int width = cell.width > 1 ? 2 : 1;
    const int pxW = width * cellW;
    const int x = originX_ + col * cellW;
    const int y = originY_ + row * cellH;

    uint32_t fg = terminal.resolveColor(cell.fg, true);
    uint32_t bg = terminal.resolveColor(cell.bg, false);
    const bool reverse = (cell.attrs.reverse != 0) != terminal.screenReverse();
    if (reverse) {
        std::swap(fg, bg);
    }
    if (cell.attrs.conceal) {
        fg = bg;
    }

    fillRect(x, y, pxW, cellH, bg);

    if (cell.chars[0] != 0 && cell.chars[0] != static_cast<uint32_t>(' ')) {
        drawGlyphRun(terminal, cell, x, y, pxW, fg);
    }

    const int thickness = std::max(1, cellH / 16);
    if (cell.attrs.underline != 0) {
        int uy = y + m.baseline + 2;
        if (uy > y + cellH - thickness) {
            uy = y + cellH - thickness;
        }
        fillRect(x, uy, pxW, thickness, fg);
        if (cell.attrs.underline == VTERM_UNDERLINE_DOUBLE) {
            const int uy2 = std::min(y + cellH - thickness, uy + thickness + 1);
            fillRect(x, uy2, pxW, thickness, fg);
        }
    }
    if (cell.attrs.strike) {
        fillRect(x, y + cellH / 2, pxW, thickness, fg);
    }

    if (cell.attrs.blink) {
        // Text blink is intentionally not animated: it is rare and would
        // require a second full damage pass every 500 ms.
    }
}

void Renderer::drawGlyphRun(Terminal& terminal, const VTermScreenCell& cell, int x, int y,
                            int cellPixelWidth, uint32_t fg)
{
    (void)terminal;
    const FontMetrics& m = font_.metrics();
    const bool bold = cell.attrs.bold != 0;
    const bool italic = cell.attrs.italic != 0;

    int penX = x;
    bool first = true;
    for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL; ++i) {
        const uint32_t cp = cell.chars[i];
        if (cp == 0) {
            break;
        }

        const Glyph* glyph = nullptr;
        if (!font_.getGlyph(cp, bold, italic, &glyph) || glyph == nullptr) {
            if (cp != static_cast<uint32_t>(' ')) {
                drawHollowBox(x + 2, y + 2, std::max(1, cellPixelWidth - 4),
                              std::max(1, m.cellHeight - 4), fg);
            }
            break;
        }

        if (first) {
            penX = x + (cellPixelWidth - glyph->advance) / 2;
            first = false;
        }

        if (glyph->width > 0 && glyph->height > 0) {
            const int gx = penX + glyph->left;
            const int gy = y + m.baseline - glyph->top;
            blendGlyph(gx, gy, *glyph, fg, x, y, cellPixelWidth, m.cellHeight);
        }

        penX += glyph->advance;
    }
}

void Renderer::drawHollowBox(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    const int t = std::max(1, font_.metrics().cellHeight / 16);
    if (w <= 2 * t || h <= 2 * t) {
        fillRect(x, y, w, h, color);
        return;
    }
    fillRect(x, y, w, t, color);
    fillRect(x, y + h - t, w, t, color);
    fillRect(x, y, t, h, color);
    fillRect(x + w - t, y, t, h, color);
}

void Renderer::fillRect(int x, int y, int w, int h, uint32_t color)
{
    if (w <= 0 || h <= 0 || canvas_.pixels() == nullptr) {
        return;
    }
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(canvas_.width(), x + w);
    const int y1 = std::min(canvas_.height(), y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t* p = canvas_.pixels() + static_cast<size_t>(row) * canvas_.stridePixels() +
                      static_cast<size_t>(x0);
        for (int col = x0; col < x1; ++col) {
            p[col - x0] = color;
        }
    }
}

void Renderer::blendGlyph(int x, int y, const Glyph& glyph, uint32_t color, int clipX,
                          int clipY, int clipW, int clipH)
{
    if (glyph.width <= 0 || glyph.height <= 0 || canvas_.pixels() == nullptr) {
        return;
    }

    const int cx0 = std::max(clipX, 0);
    const int cy0 = std::max(clipY, 0);
    const int cx1 = std::min(clipX + clipW, canvas_.width());
    const int cy1 = std::min(clipY + clipH, canvas_.height());
    if (cx0 >= cx1 || cy0 >= cy1) {
        return;
    }

    for (int gy = 0; gy < glyph.height; ++gy) {
        const int dy = y + gy;
        if (dy < cy0 || dy >= cy1) {
            continue;
        }
        const uint8_t* srcRow =
            glyph.bitmap.data() + static_cast<size_t>(gy) * static_cast<size_t>(glyph.width);
        uint32_t* dstRow = canvas_.pixels() +
                           static_cast<size_t>(dy) * canvas_.stridePixels() +
                           static_cast<size_t>(cx0);

        for (int gx = 0; gx < glyph.width; ++gx) {
            const int dx = x + gx;
            if (dx < cx0 || dx >= cx1) {
                continue;
            }
            const uint8_t alpha = srcRow[gx];
            if (alpha == 0) {
                continue;
            }
            uint32_t& dst = dstRow[dx - cx0];
            if (alpha == 255) {
                dst = color;
            } else {
                dst = blendPixel(dst, color, alpha);
            }
        }
    }
}

void Renderer::invertRect(int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0 || canvas_.pixels() == nullptr) {
        return;
    }
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(canvas_.width(), x + w);
    const int y1 = std::min(canvas_.height(), y + h);
    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int row = y0; row < y1; ++row) {
        uint32_t* p = canvas_.pixels() + static_cast<size_t>(row) * canvas_.stridePixels() +
                      static_cast<size_t>(x0);
        for (int col = x0; col < x1; ++col) {
            p[col - x0] ^= 0x00ffffffu;
        }
    }
}

bool Renderer::applyMoves(Terminal& terminal)
{
    for (size_t i = 0; i < terminal.moves().size(); ++i) {
        if (!movePixels(terminal, terminal.moves()[i])) {
            terminal.markFull();
            return false;
        }
    }
    return true;
}

bool Renderer::movePixels(Terminal& terminal, const MoveRect& move)
{
    const FontMetrics& m = font_.metrics();
    const int cellW = std::max(1, m.cellWidth);
    const int cellH = std::max(1, m.cellHeight);
    const int destW = move.dest.col1 - move.dest.col0;
    const int destH = move.dest.row1 - move.dest.row0;
    if (destW <= 0 || destH <= 0) {
        return true;
    }
    if (move.src.row1 - move.src.row0 != destH || move.src.col1 - move.src.col0 != destW) {
        return false;
    }
    if (move.dest.row0 < 0 || move.dest.row1 > terminal.rows() || move.dest.col0 < 0 ||
        move.dest.col1 > terminal.cols() || move.src.row0 < 0 ||
        move.src.row1 > terminal.rows() || move.src.col0 < 0 ||
        move.src.col1 > terminal.cols()) {
        return false;
    }

    const int dx = (move.dest.col0 - move.src.col0) * cellW;
    const int dy = (move.dest.row0 - move.src.row0) * cellH;
    if (dx == 0 && dy == 0) {
        return true;
    }

    const int wpx = destW * cellW;
    const int hpx = destH * cellH;
    const int destX = originX_ + move.dest.col0 * cellW;
    const int destY = originY_ + move.dest.row0 * cellH;
    if (destX < 0 || destY < 0 || destX + wpx > canvas_.width() ||
        destY + hpx > canvas_.height()) {
        return false;
    }

    const int srcX = destX - dx;
    const int srcY = destY - dy;
    if (srcX < 0 || srcY < 0 || srcX + wpx > canvas_.width() ||
        srcY + hpx > canvas_.height()) {
        return false;
    }

    uint32_t* pixels = canvas_.pixels();
    const int stride = canvas_.stridePixels();

    if (dy > 0) {
        for (int row = hpx - 1; row >= 0; --row) {
            uint32_t* dst = pixels + static_cast<size_t>(destY + row) * stride + destX;
            const uint32_t* src = pixels + static_cast<size_t>(srcY + row) * stride + srcX;
            std::memmove(dst, src, static_cast<size_t>(wpx) * sizeof(uint32_t));
        }
    } else {
        for (int row = 0; row < hpx; ++row) {
            uint32_t* dst = pixels + static_cast<size_t>(destY + row) * stride + destX;
            const uint32_t* src = pixels + static_cast<size_t>(srcY + row) * stride + srcX;
            std::memmove(dst, src, static_cast<size_t>(wpx) * sizeof(uint32_t));
        }
    }

    return true;
}

void Renderer::drawCursor(Terminal& terminal)
{
    if (!terminal.cursorVisible() || terminal.scrollOffset() != 0 || !cursorOn_) {
        return;
    }

    const VTermPos pos = terminal.cursor();
    VTermScreenCell cell;
    if (!terminal.getViewCell(pos.row, pos.col, &cell)) {
        return;
    }

    const FontMetrics& m = font_.metrics();
    const int cellW = std::max(1, m.cellWidth);
    const int cellH = std::max(1, m.cellHeight);
    const int width = cell.width > 1 ? 2 : 1;
    const int x = originX_ + pos.col * cellW;
    const int y = originY_ + pos.row * cellH;
    const int w = width * cellW;

    if (!cursorPatch_.empty()) {
        eraseCursor();
    }

    saveCursorPatch(x, y, w, cellH);
    if (cursorPatch_.empty()) {
        return;
    }

    switch (terminal.cursorShape()) {
    case VTERM_PROP_CURSORSHAPE_UNDERLINE:
        fillRect(x, y + cellH - std::max(1, cellH / 16), w,
                 std::max(1, cellH / 16), kCursorColor);
        break;
    case VTERM_PROP_CURSORSHAPE_BAR_LEFT:
        fillRect(x, y, std::max(1, cellW / 8), cellH, kCursorColor);
        break;
    case VTERM_PROP_CURSORSHAPE_BLOCK:
    default:
        invertRect(x, y, w, cellH);
        break;
    }

    addInvalid(x, y, w, cellH);
}

void Renderer::saveCursorPatch(int x, int y, int w, int h)
{
    const int x0 = std::max(0, x);
    const int y0 = std::max(0, y);
    const int x1 = std::min(canvas_.width(), x + w);
    const int y1 = std::min(canvas_.height(), y + h);
    cursorPatch_.clear();
    cursorPatchW_ = x1 - x0;
    cursorPatchH_ = y1 - y0;
    cursorPatchX_ = x0;
    cursorPatchY_ = y0;
    if (cursorPatchW_ <= 0 || cursorPatchH_ <= 0) {
        cursorPatchW_ = 0;
        cursorPatchH_ = 0;
        return;
    }

    cursorPatch_.resize(static_cast<size_t>(cursorPatchW_) *
                        static_cast<size_t>(cursorPatchH_));
    for (int row = 0; row < cursorPatchH_; ++row) {
        const uint32_t* src = canvas_.pixels() +
                              static_cast<size_t>(y0 + row) * canvas_.stridePixels() +
                              static_cast<size_t>(x0);
        std::memcpy(&cursorPatch_[static_cast<size_t>(row) * cursorPatchW_], src,
                    static_cast<size_t>(cursorPatchW_) * sizeof(uint32_t));
    }
}

void Renderer::eraseCursor()
{
    if (cursorPatch_.empty() || cursorPatchW_ <= 0 || cursorPatchH_ <= 0) {
        cursorPatch_.clear();
        cursorPatchW_ = 0;
        cursorPatchH_ = 0;
        return;
    }

    for (int row = 0; row < cursorPatchH_; ++row) {
        uint32_t* dst = canvas_.pixels() +
                        static_cast<size_t>(cursorPatchY_ + row) * canvas_.stridePixels() +
                        static_cast<size_t>(cursorPatchX_);
        std::memcpy(dst, &cursorPatch_[static_cast<size_t>(row) * cursorPatchW_],
                    static_cast<size_t>(cursorPatchW_) * sizeof(uint32_t));
    }
    addInvalid(cursorPatchX_, cursorPatchY_, cursorPatchW_, cursorPatchH_);

    cursorPatch_.clear();
    cursorPatchW_ = 0;
    cursorPatchH_ = 0;
}

void Renderer::wakeCursor(Terminal& terminal)
{
    if (cursorOn_) {
        return;
    }
    invalid_.clear();
    invalidAll_ = false;
    eraseCursor();
    cursorOn_ = true;
    drawCursor(terminal);
    flushInvalid();
}

void Renderer::toggleCursor(Terminal& terminal)
{
    invalid_.clear();
    invalidAll_ = false;

    eraseCursor();

    if (!terminal.cursorVisible() || terminal.scrollOffset() != 0) {
        cursorOn_ = true;
        flushInvalid();
        return;
    }

    cursorOn_ = !cursorOn_;
    if (cursorOn_) {
        drawCursor(terminal);
    }
    flushInvalid();
}

void Renderer::addInvalid(int x, int y, int w, int h)
{
    if (invalidAll_ || w <= 0 || h <= 0) {
        return;
    }
    Rect rect;
    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;
    invalid_.push_back(rect);
}

void Renderer::addInvalidAll()
{
    invalidAll_ = true;
    invalid_.clear();
}

void Renderer::flushInvalid()
{
    if (invalidAll_) {
        canvas_.invalidateAll();
    } else {
        if (!invalid_.empty()) {
            canvas_.dropImageCache();
        }
        for (size_t i = 0; i < invalid_.size(); ++i) {
            canvas_.invalidate(invalid_[i].x, invalid_[i].y, invalid_[i].w,
                               invalid_[i].h);
        }
    }
    invalid_.clear();
    invalidAll_ = false;
}

} // namespace lvfb
