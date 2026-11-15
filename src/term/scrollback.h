#ifndef LVFB_TERM_SCROLLBACK_H
#define LVFB_TERM_SCROLLBACK_H

#include <vterm.h>

#include <cstddef>
#include <deque>
#include <vector>

namespace lvfb {

struct HistoryLine {
    int cols = 0;
    std::vector<VTermScreenCell> cells;
};

VTermScreenCell blankCell();

class Scrollback {
public:
    explicit Scrollback(size_t limit);
    ~Scrollback();

    Scrollback(const Scrollback&) = delete;
    Scrollback& operator=(const Scrollback&) = delete;

    void setLimit(size_t limit);
    size_t limit() const { return limit_; }
    size_t size() const { return lines_.size(); }
    bool empty() const { return lines_.empty(); }

    void push(int cols, const VTermScreenCell* cells);
    bool pop(int cols, VTermScreenCell* cells);
    void clear();

    const HistoryLine& at(size_t index) const { return lines_[index]; }

private:
    std::deque<HistoryLine> lines_;
    size_t limit_ = 0;
};

} // namespace lvfb

#endif // LVFB_TERM_SCROLLBACK_H
