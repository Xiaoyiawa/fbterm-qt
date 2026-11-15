#include "term/scrollback.h"

#include <cstring>

namespace lvfb {

VTermScreenCell blankCell()
{
    VTermScreenCell cell;
    std::memset(&cell, 0, sizeof(cell));
    cell.chars[0] = static_cast<uint32_t>(' ');
    cell.width = 1;
    cell.fg.type = VTERM_COLOR_DEFAULT_FG;
    cell.bg.type = VTERM_COLOR_DEFAULT_BG;
    return cell;
}

Scrollback::Scrollback(size_t limit)
    : limit_(limit)
{
}

Scrollback::~Scrollback() = default;

void Scrollback::setLimit(size_t limit)
{
    limit_ = limit;
    while (lines_.size() > limit_) {
        lines_.pop_front();
    }
}

void Scrollback::push(int cols, const VTermScreenCell* cells)
{
    if (limit_ == 0 || cols <= 0) {
        return;
    }

    HistoryLine line;
    line.cols = cols;
    line.cells.reserve(static_cast<size_t>(cols));
    if (cells != nullptr) {
        for (int i = 0; i < cols; ++i) {
            line.cells.push_back(cells[i]);
        }
    } else {
        for (int i = 0; i < cols; ++i) {
            line.cells.push_back(blankCell());
        }
    }

    lines_.push_back(std::move(line));
    while (lines_.size() > limit_) {
        lines_.pop_front();
    }
}

bool Scrollback::pop(int cols, VTermScreenCell* cells)
{
    if (lines_.empty() || cols <= 0 || cells == nullptr) {
        return false;
    }

    const HistoryLine& line = lines_.back();
    const int copyCols = line.cols < cols ? line.cols : cols;
    for (int i = 0; i < copyCols; ++i) {
        cells[i] = line.cells[static_cast<size_t>(i)];
    }
    for (int i = copyCols; i < cols; ++i) {
        cells[i] = blankCell();
    }

    lines_.pop_back();
    return true;
}

void Scrollback::clear()
{
    lines_.clear();
}

} // namespace lvfb
