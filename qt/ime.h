#ifndef FBTERM_QT_IME_H
#define FBTERM_QT_IME_H

#include <QString>
#include <Qt>

struct pinyin_ime_t;

// Small state machine around libpinyinime. It never draws a mode icon;
// display() is only non-empty while there is a composition to show.
class PinyinIme {
public:
    PinyinIme();
    ~PinyinIme();

    bool init();
    bool enabled() const { return engine_ != nullptr; }
    bool chineseMode() const { return chinese_; }
    void toggleMode();

    // Returns true when the key was consumed. 'commit' is UTF-8 text that
    // should be sent to the terminal.
    bool handleKey(int key, Qt::KeyboardModifiers mods, const QString& text,
                   QString* commit);
    QString display() const;

private:
    void update();
    void reset();
    bool select(int index, QString* commit);
    QString currentResult() const;
    QString currentCommit() const;
    QString segments() const;
    bool hasComposition() const;

    pinyin_ime_t* engine_ = nullptr;
    QString buffer_;
    bool chinese_ = true;
    int page_ = 0;
};

#endif
