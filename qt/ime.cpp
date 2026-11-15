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
    if (buffer_.isEmpty()) {
        pinyin_ime_input(engine_, "");
    } else {
        pinyin_ime_input(engine_, buffer_.toUtf8().constData());
    }
}

void PinyinIme::reset()
{
    buffer_.clear();
    if (engine_ != nullptr) pinyin_ime_input(engine_, "");
}

QString PinyinIme::currentResult() const
{
    if (engine_ == nullptr) return QString();
    const char* result = pinyin_ime_get_result(engine_);
    return result != nullptr ? QString::fromUtf8(result) : QString();
}

QString PinyinIme::currentCommit() const
{
    const QString result = currentResult();
    return result.isEmpty() ? buffer_ : result;
}

bool PinyinIme::select(int index, QString* commit)
{
    if (engine_ == nullptr || index < 0) return false;
    const int count = pinyin_ime_get_candidate_count(engine_);
    if (index >= count) return false;
    if (pinyin_ime_select(engine_, static_cast<unsigned int>(index)) != PINYIN_IME_OK) {
        return false;
    }
    // Commit the whole result even if more segments remain; this is a
    // terminal, not a full editor, and keeps the state predictable.
    *commit = currentResult();
    reset();
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
        if (buffer_.isEmpty()) return false;
        buffer_.chop(1);
        update();
        return true;
    }
    if (key == Qt::Key_Escape) {
        if (buffer_.isEmpty()) return false;
        reset();
        return true;
    }
    if (key == Qt::Key_Return || key == Qt::Key_Enter) {
        if (buffer_.isEmpty()) return false;
        *commit = buffer_;
        reset();
        return true;
    }
    if (key == Qt::Key_Space) {
        if (buffer_.isEmpty()) return false;
        if (pinyin_ime_get_candidate_count(engine_) > 0) {
            select(0, commit);
        } else {
            *commit = buffer_;
            reset();
        }
        return true;
    }
    if (key >= Qt::Key_1 && key <= Qt::Key_9 && !buffer_.isEmpty()) {
        const int index = key - Qt::Key_1;
        if (pinyin_ime_get_candidate_count(engine_) > index) {
            select(index, commit);
        }
        return true;
    }

    if (key >= Qt::Key_A && key <= Qt::Key_Z) {
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
        if (!buffer_.isEmpty()) {
            *commit = currentCommit();
            reset();
        }
        *commit += fullWidth(c);
        return true;
    }

    // While composing, swallow navigation/editing keys instead of letting
    // QTermWidget move the cursor under the overlay.
    if (!buffer_.isEmpty()) return true;
    return false;
}

QString PinyinIme::display() const
{
    if (engine_ == nullptr || buffer_.isEmpty()) return QString();
    QString out = buffer_;
    const int count = pinyin_ime_get_candidate_count(engine_);
    if (count <= 0) {
        out += QStringLiteral("  (无候选)");
        return out;
    }
    const int shown = std::min(count, 9);
    for (int i = 0; i < shown; ++i) {
        const char* cand = pinyin_ime_get_candidate(engine_, static_cast<unsigned int>(i));
        if (cand == nullptr) continue;
        out += QStringLiteral("  %1.%2").arg(i + 1).arg(QString::fromUtf8(cand));
    }
    return out;
}
