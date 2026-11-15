#include "term/terminal.h"

#include "app/log.h"
#include "term/pty.h"

#include <algorithm>
#include <cstring>

namespace lvfb {

namespace {

const VTermScreenCallbacks kScreenCallbacks = {
    Terminal::damageCb,
    Terminal::moverectCb,
    Terminal::movecursorCb,
    Terminal::settermpropCb,
    Terminal::bellCb,
    Terminal::resizeCb,
    Terminal::sbPushlineCb,
    Terminal::sbPoplineCb,
    Terminal::sbClearCb,
    nullptr, // sb_pushline4
};

} // namespace

Terminal::Terminal(int rows, int cols, int scrollbackLimit)
    : rows_(rows > 0 ? rows : 1)
    , cols_(cols > 0 ? cols : 1)
    , scrollback_(scrollbackLimit > 0 ? static_cast<size_t>(scrollbackLimit) : 0)
{
    reallocDamage();
}

Terminal::~Terminal()
{
    if (vt_ != nullptr) {
        vterm_free(vt_);
        vt_ = nullptr;
        state_ = nullptr;
        screen_ = nullptr;
    }
}

bool Terminal::init(std::string* error)
{
    vt_ = vterm_new(rows_, cols_);
    if (vt_ == nullptr) {
        if (error != nullptr) {
            *error = "vterm_new() failed";
        }
        return false;
    }

    vterm_set_utf8(vt_, 1);
    state_ = vterm_obtain_state(vt_);
    screen_ = vterm_obtain_screen(vt_);
    if (state_ == nullptr || screen_ == nullptr) {
        if (error != nullptr) {
            *error = "vterm_obtain_state/screen failed";
        }
        return false;
    }

    vterm_screen_set_callbacks(screen_, &kScreenCallbacks, this);
    vterm_screen_enable_altscreen(screen_, 1);
    vterm_screen_enable_reflow(screen_, false);
    vterm_screen_set_damage_merge(screen_, VTERM_DAMAGE_CELL);
    vterm_state_set_bold_highbright(state_, 1);

    VTermColor fg;
    VTermColor bg;
    std::memset(&fg, 0, sizeof(fg));
    std::memset(&bg, 0, sizeof(bg));
    fg.type = VTERM_COLOR_RGB;
    fg.rgb.red = 0xd0;
    fg.rgb.green = 0xd0;
    fg.rgb.blue = 0xd0;
    bg.type = VTERM_COLOR_RGB;
    bg.rgb.red = 0x00;
    bg.rgb.green = 0x00;
    bg.rgb.blue = 0x00;
    vterm_screen_set_default_colors(screen_, &fg, &bg);

    vterm_output_set_callback(vt_, &Terminal::outputCb, this);
    vterm_screen_reset(screen_, 1);

    full_ = true;
    cursorVisible_ = true;
    cursorBlink_ = true;
    cursorShape_ = VTERM_PROP_CURSORSHAPE_BLOCK;
    mouseMode_ = VTERM_PROP_MOUSE_NONE;
    altScreen_ = false;
    screenReverse_ = false;
    scrollOffset_ = 0;
    cursor_.row = 0;
    cursor_.col = 0;

    return true;
}

void Terminal::reallocDamage()
{
    dirtyMin_.assign(static_cast<size_t>(rows_), cols_);
    dirtyMax_.assign(static_cast<size_t>(rows_), -1);
}

void Terminal::feed(const char* data, size_t len)
{
    if (data == nullptr || len == 0 || vt_ == nullptr) {
        return;
    }

    resetScroll();
    vterm_input_write(vt_, data, len);
}

void Terminal::process()
{
    if (screen_ != nullptr) {
        vterm_screen_flush_damage(screen_);
    }
}

void Terminal::resize(int rows, int cols)
{
    if (rows < 1) {
        rows = 1;
    }
    if (cols < 1) {
        cols = 1;
    }
    if (rows == rows_ && cols == cols_) {
        return;
    }

    rows_ = rows;
    cols_ = cols;
    reallocDamage();
    scrollOffset_ = 0;
    moves_.clear();
    full_ = false;

    vterm_set_size(vt_, rows_, cols_);

    if (pty_ != nullptr) {
        pty_->resize(rows_, cols_);
    }

    markFull();
    LVFB_LOGI("terminal resized to %dx%d", cols_, rows_);
}

void Terminal::reset()
{
    if (screen_ != nullptr) {
        vterm_screen_reset(screen_, 1);
    }
    scrollback_.clear();
    scrollOffset_ = 0;
    cursor_.row = 0;
    cursor_.col = 0;
    cursorVisible_ = true;
    cursorBlink_ = true;
    cursorShape_ = VTERM_PROP_CURSORSHAPE_BLOCK;
    mouseMode_ = VTERM_PROP_MOUSE_NONE;
    altScreen_ = false;
    screenReverse_ = false;
    title_.clear();
    reallocDamage();
    markFull();
}

bool Terminal::hasDamage() const
{
    if (full_) {
        return true;
    }
    for (int row = 0; row < rows_; ++row) {
        if (dirtyMin_[static_cast<size_t>(row)] <= dirtyMax_[static_cast<size_t>(row)]) {
            return true;
        }
    }
    return false;
}

void Terminal::markFull()
{
    full_ = true;
    moves_.clear();
}

void Terminal::clearDamage()
{
    full_ = false;
    moves_.clear();
    for (int row = 0; row < rows_; ++row) {
        dirtyMin_[static_cast<size_t>(row)] = cols_;
        dirtyMax_[static_cast<size_t>(row)] = -1;
    }
}

int Terminal::dirtyMinCol(int row) const
{
    if (row < 0 || row >= rows_) {
        return -1;
    }
    if (full_) {
        return 0;
    }
    const int minCol = dirtyMin_[static_cast<size_t>(row)];
    return minCol <= dirtyMax_[static_cast<size_t>(row)] ? minCol : -1;
}

int Terminal::dirtyMaxCol(int row) const
{
    if (row < 0 || row >= rows_) {
        return -1;
    }
    if (full_) {
        return cols_ - 1;
    }
    const int maxCol = dirtyMax_[static_cast<size_t>(row)];
    return dirtyMin_[static_cast<size_t>(row)] <= maxCol ? maxCol : -1;
}

void Terminal::markCell(int row, int col)
{
    if (full_ || row < 0 || row >= rows_ || col < 0 || col >= cols_) {
        return;
    }
    int& minCol = dirtyMin_[static_cast<size_t>(row)];
    int& maxCol = dirtyMax_[static_cast<size_t>(row)];
    if (col < minCol) {
        minCol = col;
    }
    if (col > maxCol) {
        maxCol = col;
    }
}

void Terminal::markRect(const CellRect& rect)
{
    if (full_) {
        return;
    }
    const int row0 = std::max(0, rect.row0);
    const int row1 = std::min(rows_, rect.row1);
    const int col0 = std::max(0, rect.col0);
    const int col1 = std::min(cols_, rect.col1);
    if (row0 >= row1 || col0 >= col1) {
        return;
    }
    for (int row = row0; row < row1; ++row) {
        int& minCol = dirtyMin_[static_cast<size_t>(row)];
        int& maxCol = dirtyMax_[static_cast<size_t>(row)];
        if (col0 < minCol) {
            minCol = col0;
        }
        if (col1 - 1 > maxCol) {
            maxCol = col1 - 1;
        }
    }
}

void Terminal::markCursorCell(VTermPos pos)
{
    markCell(pos.row, pos.col);
    // A wide character under the cursor spans two cells.
    if (screen_ != nullptr) {
        VTermScreenCell cell;
        std::memset(&cell, 0, sizeof(cell));
        if (vterm_screen_get_cell(screen_, pos, &cell) != 0 && cell.width > 1) {
            markCell(pos.row, pos.col + 1);
        }
    }
}

int Terminal::damageCb(VTermRect rect, void* user)
{
    return static_cast<Terminal*>(user)->onDamage(rect);
}

int Terminal::moverectCb(VTermRect dest, VTermRect src, void* user)
{
    return static_cast<Terminal*>(user)->onMoverect(dest, src);
}

int Terminal::movecursorCb(VTermPos pos, VTermPos oldpos, int visible, void* user)
{
    return static_cast<Terminal*>(user)->onMovecursor(pos, oldpos, visible);
}

int Terminal::settermpropCb(VTermProp prop, VTermValue* val, void* user)
{
    return static_cast<Terminal*>(user)->onSettermprop(prop, val);
}

int Terminal::bellCb(void* user)
{
    (void)user;
    return 1;
}

int Terminal::resizeCb(int rows, int cols, void* user)
{
    return static_cast<Terminal*>(user)->onResize(rows, cols);
}

int Terminal::sbPushlineCb(int cols, const VTermScreenCell* cells, void* user)
{
    return static_cast<Terminal*>(user)->onSbPushline(cols, cells);
}

int Terminal::sbPoplineCb(int cols, VTermScreenCell* cells, void* user)
{
    return static_cast<Terminal*>(user)->onSbPopline(cols, cells);
}

int Terminal::sbClearCb(void* user)
{
    return static_cast<Terminal*>(user)->onSbClear();
}

void Terminal::outputCb(const char* s, size_t len, void* user)
{
    static_cast<Terminal*>(user)->onOutput(s, len);
}

int Terminal::onDamage(VTermRect rect)
{
    if (full_) {
        return 1;
    }
    CellRect r;
    r.row0 = rect.start_row;
    r.row1 = rect.end_row;
    r.col0 = rect.start_col;
    r.col1 = rect.end_col;
    markRect(r);
    return 1;
}

int Terminal::onMoverect(VTermRect dest, VTermRect src)
{
    if (full_) {
        return 1;
    }

    MoveRect move;
    move.dest.row0 = dest.start_row;
    move.dest.row1 = dest.end_row;
    move.dest.col0 = dest.start_col;
    move.dest.col1 = dest.end_col;
    move.src.row0 = src.start_row;
    move.src.row1 = src.end_row;
    move.src.col0 = src.start_col;
    move.src.col1 = src.end_col;

    const int dy = dest.start_row - src.start_row;
    const int dx = dest.start_col - src.start_col;

    if (dy > 0) {
        CellRect exposed(dest.start_row, dest.start_row + dy, dest.start_col, dest.end_col);
        markRect(exposed);
    } else if (dy < 0) {
        CellRect exposed(dest.end_row + dy, dest.end_row, dest.start_col, dest.end_col);
        markRect(exposed);
    }
    if (dx > 0) {
        CellRect exposed(dest.start_row, dest.end_row, dest.start_col, dest.start_col + dx);
        markRect(exposed);
    } else if (dx < 0) {
        CellRect exposed(dest.start_row, dest.end_row, dest.end_col + dx, dest.end_col);
        markRect(exposed);
    }

    moves_.push_back(move);
    if (moves_.size() > 64) {
        markFull();
    }
    return 1;
}

int Terminal::onMovecursor(VTermPos pos, VTermPos oldpos, int visible)
{
    markCursorCell(oldpos);
    markCursorCell(pos);
    cursor_ = pos;
    if (visible >= 0) {
        cursorVisible_ = visible != 0;
    }
    return 1;
}

int Terminal::onSettermprop(VTermProp prop, VTermValue* val)
{
    if (val == nullptr) {
        return 1;
    }

    switch (prop) {
    case VTERM_PROP_CURSORVISIBLE:
        cursorVisible_ = val->boolean != 0;
        markCursorCell(cursor_);
        break;
    case VTERM_PROP_CURSORBLINK:
        cursorBlink_ = val->boolean != 0;
        markCursorCell(cursor_);
        break;
    case VTERM_PROP_ALTSCREEN:
        if (altScreen_ != (val->boolean != 0)) {
            altScreen_ = val->boolean != 0;
            scrollOffset_ = 0;
            markFull();
        }
        break;
    case VTERM_PROP_REVERSE:
        if (screenReverse_ != (val->boolean != 0)) {
            screenReverse_ = val->boolean != 0;
            markFull();
        }
        break;
    case VTERM_PROP_CURSORSHAPE: {
        int shape = val->number;
        if (shape < VTERM_PROP_CURSORSHAPE_BLOCK || shape > VTERM_PROP_CURSORSHAPE_BAR_LEFT) {
            shape = VTERM_PROP_CURSORSHAPE_BLOCK;
        }
        if (cursorShape_ != shape) {
            cursorShape_ = shape;
            markCursorCell(cursor_);
        }
        break;
    }
    case VTERM_PROP_MOUSE:
        mouseMode_ = val->number;
        break;
    case VTERM_PROP_TITLE:
        if (val->string.initial) {
            title_.clear();
        }
        if (val->string.str != nullptr && val->string.len > 0) {
            title_.append(val->string.str, val->string.len);
        }
        break;
    case VTERM_PROP_ICONNAME:
    case VTERM_PROP_FOCUSREPORT:
    default:
        break;
    }

    return 1;
}

int Terminal::onResize(int rows, int cols)
{
    if (rows < 1 || cols < 1) {
        return 1;
    }
    rows_ = rows;
    cols_ = cols;
    reallocDamage();
    scrollOffset_ = 0;
    markFull();
    return 1;
}

int Terminal::onSbPushline(int cols, const VTermScreenCell* cells)
{
    scrollback_.push(cols, cells);
    return 1;
}

int Terminal::onSbPopline(int cols, VTermScreenCell* cells)
{
    return scrollback_.pop(cols, cells) ? 1 : 0;
}

int Terminal::onSbClear()
{
    scrollback_.clear();
    return 1;
}

void Terminal::onOutput(const char* s, size_t len)
{
    if (pty_ != nullptr && s != nullptr && len > 0) {
        pty_->writeAll(s, len);
    }
}

bool Terminal::getCell(int row, int col, VTermScreenCell* cell) const
{
    if (screen_ == nullptr || cell == nullptr || row < 0 || row >= rows_ || col < 0 ||
        col >= cols_) {
        return false;
    }
    VTermPos pos;
    pos.row = row;
    pos.col = col;
    return vterm_screen_get_cell(screen_, pos, cell) != 0;
}

bool Terminal::getViewCell(int row, int col, VTermScreenCell* cell) const
{
    if (cell == nullptr || row < 0 || row >= rows_ || col < 0 || col >= cols_) {
        return false;
    }

    const int virtualRow = row - scrollOffset_;
    if (virtualRow >= 0) {
        return getCell(virtualRow, col, cell);
    }

    if (scrollback_.empty()) {
        *cell = blankCell();
        return true;
    }

    const int index = static_cast<int>(scrollback_.size()) + virtualRow;
    if (index < 0 || index >= static_cast<int>(scrollback_.size())) {
        *cell = blankCell();
        return true;
    }

    const HistoryLine& line = scrollback_.at(static_cast<size_t>(index));
    if (col >= line.cols) {
        *cell = blankCell();
    } else {
        *cell = line.cells[static_cast<size_t>(col)];
    }
    return true;
}

void Terminal::scrollBy(int lines)
{
    if (altScreen_ || lines == 0) {
        return;
    }

    const int maxOffset = static_cast<int>(scrollback_.size());
    int next = scrollOffset_ + lines;
    if (next < 0) {
        next = 0;
    }
    if (next > maxOffset) {
        next = maxOffset;
    }
    if (next != scrollOffset_) {
        scrollOffset_ = next;
        markFull();
    }
}

void Terminal::resetScroll()
{
    if (scrollOffset_ != 0) {
        scrollOffset_ = 0;
        markFull();
    }
}

uint32_t Terminal::resolveColor(const VTermColor& color, bool foreground) const
{
    if (foreground) {
        if ((color.type & VTERM_COLOR_DEFAULT_FG) != 0) {
            return defaultFg_;
        }
    } else {
        if ((color.type & VTERM_COLOR_DEFAULT_BG) != 0) {
            return defaultBg_;
        }
    }

    VTermColor converted = color;
    if (screen_ != nullptr) {
        vterm_screen_convert_color_to_rgb(screen_, &converted);
    }
    if (converted.type == VTERM_COLOR_RGB) {
        return (static_cast<uint32_t>(converted.rgb.red) << 16) |
               (static_cast<uint32_t>(converted.rgb.green) << 8) |
               static_cast<uint32_t>(converted.rgb.blue);
    }
    return foreground ? defaultFg_ : defaultBg_;
}

void Terminal::sendKey(VTermKey key, VTermModifier mod)
{
    if (vt_ == nullptr) {
        return;
    }
    vterm_keyboard_key(vt_, key, mod);
    if (pty_ != nullptr) {
        pty_->flushWrites();
    }
}

void Terminal::sendUnichar(uint32_t codepoint, VTermModifier mod)
{
    if (vt_ == nullptr) {
        return;
    }
    vterm_keyboard_unichar(vt_, codepoint, mod);
    if (pty_ != nullptr) {
        pty_->flushWrites();
    }
}

void Terminal::writeRaw(const char* data, size_t len)
{
    if (pty_ != nullptr) {
        pty_->writeAll(data, len);
    }
}

void Terminal::sendMouseWheel(int direction, VTermModifier mod)
{
    if (vt_ == nullptr || mouseMode_ == VTERM_PROP_MOUSE_NONE || direction == 0) {
        return;
    }

    vterm_mouse_move(vt_, cursor_.row, cursor_.col, mod);
    const int button = direction > 0 ? 4 : 5;
    vterm_mouse_button(vt_, button, true, mod);
    if (pty_ != nullptr) {
        pty_->flushWrites();
    }
}

} // namespace lvfb
