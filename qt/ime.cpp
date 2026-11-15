#include "ime.h"

#include "pinyin_ime.h"

#include <QDebug>
#include <QFileInfo>
#include <QStringList>

#include <algorithm>

namespace {

QChar fullWidth(QChar c)
{
    switch (c.unicode()) {
    case ',': return QChar(0xFF0C); // ，
    case '.': return QChar(0x3002); // 。
    case '?': return QChar(0xFF1F); // ？
    case '!': return QChar(0xFF01); // ！
    case ':': return QChar(0xFF1A); // ：
    case ';': return QChar(0xFF1B); // ；
    case '(': return QChar(0xFF08); // （
    case ')': return QChar(0xFF09); // ）
    case '[': return QChar(0x3010); // 【
    case ']': return QChar(0x3011); // 】
    case '{': return QChar(0xFF5B); // ｛
    case '}': return QChar(0xFF5D); // ｝
    case '<': return QChar(0x300A); // 《
    case '>': return QChar(0x300B); // 》
    case '/': return QChar(0xFF0F); // ／
    case '\\': return QChar(0xFF3C); // ＼
    case '|': return QChar(0xFF5C); // ｜
    case '@': return QChar(0xFF20); // ＠
    case '#': return QChar(0xFF03); // ＃
    case '$': return QChar(0xFF04); // ＄
    case '%': return QChar(0xFF05); // ％
    case '^': return QChar(0xFF3E); // ＾
    case '&': return QChar(0xFF06); // ＆
    case '*': return QChar(0xFF0A); // ＊
    case '_': return QChar(0xFF3F); // ＿
    case '+': return QChar(0xFF0B); // ＋
    case '=': return QChar(0xFF1D); // ＝
    case '~': return QChar(0xFF5E); // ～
    case '`': return QChar(0xFF40); // ｀
    case '-': return QChar(0xFF0D); // －
    case '"': return QChar(0x201C); // “
    case '\'': return QChar(0x2019); // ’
    default: return QChar();
    }
}

bool isPunctuation(QChar c)
{
    return !fullWidth(c).isNull();
}

} // namespace

PinyinIme::PinyinIme() = default;

PinyinIme::~PinyinIme()
{
    if (engine_ != nullptr) pinyin_ime_destroy(engine_);
}

bool PinyinIme::init()
{
    const QStringList bases = {
        QStringLiteral("./share"),
        QStringLiteral("/root/fbterm-qt/share"),
        QStringLiteral("/usr/local/share/fbterm-qt"),
    };
    for (const QString& base : bases) {
        const QString pinyin = base + QStringLiteral("/pinyin.txt");
        const QString dict = base + QStringLiteral("/dictionary.data");
        if (!QFileInfo::exists(pinyin) || !QFileInfo::exists(dict)) continue;
        engine_ = pinyin_ime_init(pinyin.toUtf8().constData(),
                                  dict.toUtf8().constData());
        if (engine_ != nullptr) {
            qInfo() << "IME loaded:" << pinyin << dict;
            return true;
        }
    }
    qWarning() << "IME data not found (share/pinyin.txt, share/dictionary.data)";
    return false;
}

void PinyinIme::toggleMode()
{
    chinese_ = !chinese_;
    buffer_.clear();
    reset();
}

void PinyinIme::update()
{
    if (engine_ == nullptr) return;
    page_ = 0;
    if (buffer_.isEmpty()) {
        pinyin_ime_input(engine_, "");
    } else {
        pinyin_ime_input(engine_, buffer_.toUtf8().constData());
    }
}

void PinyinIme::reset()
{
    buffer_.clear();
    page_ = 0;
    if (engine_ != nullptr) pinyin_ime_input(engine_, "");
}

QString PinyinIme::currentResult() const
{
    if (engine_ == nullptr) return QString();
    const char* result = pinyin_ime_get_result(engine_);
    return result != nullptr ? QString::fromUtf8(result) : QString();
}

QString PinyinIme::segments() const
{
    if (engine_ == nullptr) return QString();
    const char* seg = pinyin_ime_get_segments(engine_);
    return seg != nullptr ? QString::fromUtf8(seg) : QString();
}

bool PinyinIme::hasComposition() const
{
    return !buffer_.isEmpty() || !currentResult().isEmpty() || !segments().isEmpty();
}

QString PinyinIme::currentCommit() const
{
    QString text = currentResult();
    QString rest = segments();
    rest.remove(QLatin1Char('\''));
    text += rest;
    if (text.isEmpty()) text = buffer_;
    return text;
}

bool PinyinIme::select(int index, QString* commit)
{
    if (engine_ == nullptr || index < 0) return false;
    const int count = pinyin_ime_get_candidate_count(engine_);
    if (index >= count) return false;
    if (pinyin_ime_select(engine_, static_cast<unsigned int>(index)) != PINYIN_IME_OK) {
        return false;
    }
    buffer_.clear();
    page_ = 0;
    // A candidate can consume the last segment while the library still
    // reports is_finished() == false; in that case there is nothing left to
    // show, so commit and leave the IME automatically.
    if (pinyin_ime_is_finished(engine_) ||
        pinyin_ime_get_candidate_count(engine_) <= 0) {
        *commit = currentResult();
        reset();
    }
    // Otherwise keep the engine state: get_segments() now returns the
    // remaining syllables (e.g. after selecting 真 from zhen'de'ma it is
    // de'ma) and candidates are regenerated for that remainder.
    return true;
}

bool PinyinIme::handleKey(int key, Qt::KeyboardModifiers mods,
                          const QString& text, QString* commit)
{
    if (engine_ == nullptr) return false;
    if (mods & (Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier)) {
        return false;
    }

    if (key == Qt::Key_Backspace) {
        if (!buffer_.isEmpty()) {
            buffer_.chop(1);
            update();
            return true;
        }
        if (hasComposition()) {
            reset();
            return true;
        }
        return false;
    }
    if (key == Qt::Key_Escape) {
        if (!hasComposition()) return false;
        reset();
        return true;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (!hasComposition()) return false;
        *commit = currentCommit();
        reset();
        return true;
    }
    if (key == Qt::Key_PageDown || key == Qt::Key_PageUp) {
        if (!hasComposition()) return false;
        const int count = pinyin_ime_get_candidate_count(engine_);
        const int pages = (count + 8) / 9;
        if (pages <= 0) return true;
        if (key == Qt::Key_PageDown) {
            if (page_ + 1 < pages) ++page_;
        } else if (page_ > 0) {
            --page_;
        }
        return true;
    }
    if (key == Qt::Key_Space) {
        if (!hasComposition()) return false;
        const int count = pinyin_ime_get_candidate_count(engine_);
        if (count > 0) {
            select(std::min(page_ * 9, count - 1), commit);
        } else {
            *commit = currentCommit();
            reset();
        }
        return true;
    }
    if (key >= Qt::Key_1 && key <= Qt::Key_9 && hasComposition()) {
        const int index = page_ * 9 + (key - Qt::Key_1);
        if (pinyin_ime_get_candidate_count(engine_) > index) {
            select(index, commit);
        }
        return true;
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
        // If a segment has already been selected, keep the engine state and
        // do not feed the letter into pinyin_ime_input() (that would reset
        // the selected result). Select the remaining segments first.
        if (hasComposition() && buffer_.isEmpty()) return true;
        if (buffer_.isEmpty() && !chinese_) return false;
        buffer_ += QChar('a' + (key - Qt::Key_A));
        update();
        return true;
    }

    if (text.isEmpty()) return false;
    const QChar c = text.at(0);

    if (c == QLatin1Char('\'') && !buffer_.isEmpty()) {
        buffer_ += c;
        update();
        return true;
    }

    if (isPunctuation(c)) {
        if (!chinese_) return false;
        if (hasComposition()) {
            *commit = currentCommit();
            reset();
        }
        *commit += fullWidth(c);
        return true;
    }

    // While composing, swallow navigation/editing keys instead of letting
    // QTermWidget move the cursor under the overlay.
    if (hasComposition()) return true;
    return false;
}

QString PinyinIme::display() const
{
    if (engine_ == nullptr || !hasComposition()) return QString();

    const QString result = currentResult();
    const QString segs = segments();
    QString out;
    if (!result.isEmpty()) out = result + QStringLiteral("  ");
    if (!segs.isEmpty()) out += segs;
    else if (!buffer_.isEmpty()) out += buffer_;
    if (out.isEmpty()) out = buffer_;

    const int count = pinyin_ime_get_candidate_count(engine_);
    if (count <= 0) {
        out += QStringLiteral("  (无候选)");
        return out;
    }
    const int pages = (count + 8) / 9;
    int page = page_;
    if (page >= pages) page = pages - 1;
    if (page < 0) page = 0;
    out += QStringLiteral("  [%1/%2]").arg(page + 1).arg(pages);
    const int begin = page * 9;
    const int end = std::min(count, begin + 9);
    for (int i = begin; i < end; ++i) {
        const char* cand = pinyin_ime_get_candidate(engine_, static_cast<unsigned int>(i));
        if (cand == nullptr) continue;
        out += QStringLiteral("  %1.%2").arg(i - begin + 1).arg(QString::fromUtf8(cand));
    }
    return out;
}
