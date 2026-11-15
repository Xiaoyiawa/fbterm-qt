#ifndef LVFB_TERM_TERMINAL_H
#define LVFB_TERM_TERMINAL_H

#include "term/scrollback.h"

#include <vterm.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace lvfb {

class Pty;

struct CellRect {
    CellRect() = default;
    CellRect(int r0, int r1, int c0, int c1) : row0(r0), row1(r1), col0(c0), col1(c1) {}

    int row0 = 0;
    int row1 = 0; // exclusive
    int col0 = 0;
    int col1 = 0; // exclusive
};

struct MoveRect {
    CellRect dest;
    CellRect src;
};

class Terminal {
public:
    Terminal(int rows, int cols, int scrollbackLimit);
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    bool init(std::string* error);
    void setPty(Pty* pty) { pty_ = pty; }

    void feed(const char* data, size_t len);
    void process(); // vterm_screen_flush_damage()

    void resize(int rows, int cols);
    void reset();

    int rows() const { return rows_; }
    int cols() const { return cols_; }

    bool hasDamage() const;
    bool fullRedraw() const { return full_; }
    void markFull();
    void clearDamage();
    int dirtyMinCol(int row) const;
    int dirtyMaxCol(int row) const;

    const std::vector<MoveRect>& moves() const { return moves_; }

    const VTermScreen* screen() const { return screen_; }
    VTermState* state() { return state_; }

    bool getCell(int row, int col, VTermScreenCell* cell) const;
    bool getViewCell(int row, int col, VTermScreenCell* cell) const;

    VTermPos cursor() const { return cursor_; }
    bool cursorVisible() const { return cursorVisible_; }
    int cursorShape() const { return cursorShape_; }
    bool cursorBlink() const { return cursorBlink_; }

    int mouseMode() const { return mouseMode_; }
    bool altScreen() const { return altScreen_; }
    bool screenReverse() const { return screenReverse_; }

    size_t historySize() const { return scrollback_.size(); }
    int scrollOffset() const { return scrollOffset_; }
    void scrollBy(int lines);
    void resetScroll();

    uint32_t resolveColor(const VTermColor& color, bool foreground) const;

    void sendKey(VTermKey key, VTermModifier mod);
    void sendUnichar(uint32_t codepoint, VTermModifier mod);
    void writeRaw(const char* data, size_t len);
    void sendMouseWheel(int direction, VTermModifier mod);

    std::string title() const { return title_; }

    // libvterm C callbacks. Public only so the C-style callback table in
    // terminal.cpp can reference them.
    static int damageCb(VTermRect rect, void* user);
    static int moverectCb(VTermRect dest, VTermRect src, void* user);
    static int movecursorCb(VTermPos pos, VTermPos oldpos, int visible, void* user);
    static int settermpropCb(VTermProp prop, VTermValue* val, void* user);
    static int bellCb(void* user);
    static int resizeCb(int rows, int cols, void* user);
    static int sbPushlineCb(int cols, const VTermScreenCell* cells, void* user);
    static int sbPoplineCb(int cols, VTermScreenCell* cells, void* user);
    static int sbClearCb(void* user);
    static void outputCb(const char* s, size_t len, void* user);

private:
    int onDamage(VTermRect rect);
    int onMoverect(VTermRect dest, VTermRect src);
    int onMovecursor(VTermPos pos, VTermPos oldpos, int visible);
    int onSettermprop(VTermProp prop, VTermValue* val);
    int onResize(int rows, int cols);
    int onSbPushline(int cols, const VTermScreenCell* cells);
    int onSbPopline(int cols, VTermScreenCell* cells);
    int onSbClear();
    void onOutput(const char* s, size_t len);

    void markCell(int row, int col);
    void markRect(const CellRect& rect);
    void markCursorCell(VTermPos pos);
    void reallocDamage();

    VTerm* vt_ = nullptr;
    VTermState* state_ = nullptr;
    VTermScreen* screen_ = nullptr;
    Pty* pty_ = nullptr;

    int rows_ = 0;
    int cols_ = 0;

    std::vector<int> dirtyMin_;
    std::vector<int> dirtyMax_;
    bool full_ = true;
    std::vector<MoveRect> moves_;

    Scrollback scrollback_;
    int scrollOffset_ = 0;

    VTermPos cursor_ = {0, 0};
    bool cursorVisible_ = true;
    bool cursorBlink_ = true;
    int cursorShape_ = VTERM_PROP_CURSORSHAPE_BLOCK;
    int mouseMode_ = VTERM_PROP_MOUSE_NONE;
    bool altScreen_ = false;
    bool screenReverse_ = false;
    std::string title_;

    uint32_t defaultFg_ = 0x00d0d0d0;
    uint32_t defaultBg_ = 0x00000000;
};

} // namespace lvfb

#endif // LVFB_TERM_TERMINAL_H
