#include "options.h"

#include <QFileInfo>
#include <QTextStream>

static bool parseInt(const QString& text, int minValue, int maxValue, int* out)
{
    bool ok = false;
    const int value = text.toInt(&ok);
    if (!ok || value < minValue || value > maxValue) return false;
    *out = value;
    return true;
}

QStringList splitCommandLine(const QString& command)
{
    QStringList out;
    QString current;
    bool single = false, dbl = false, escaped = false;
    for (int i = 0; i < command.size(); ++i) {
        const QChar ch = command[i];
        if (escaped) { current += ch; escaped = false; continue; }
        if (single) { if (ch == '\'') single = false; else current += ch; continue; }
        if (dbl) {
            if (ch == '"') dbl = false;
            else if (ch == '\\') escaped = true;
            else current += ch;
            continue;
        }
        if (ch == '\\') escaped = true;
        else if (ch == '\'') single = true;
        else if (ch == '"') dbl = true;
        else if (ch.isSpace()) {
            if (!current.isEmpty()) { out << current; current.clear(); }
        } else current += ch;
    }
    if (!current.isEmpty()) out << current;
    return out;
}

QString resolveFontPath(const QString& explicitPath)
{
    if (!explicitPath.isEmpty()) return explicitPath;
    const QStringList candidates = {
        QStringLiteral("./unifont.otf"),
        QStringLiteral("./share/unifont.otf"),
        QStringLiteral("/root/fbterm-qt/share/unifont.otf"),
        QStringLiteral("/usr/local/share/fbterm-qt/unifont.otf"),
        QStringLiteral("/usr/share/fonts/opentype/unifont/unifont.otf"),
        QStringLiteral("/usr/share/fonts/truetype/unifont/unifont.otf"),
    };
    for (const QString& path : candidates) {
        if (QFileInfo::exists(path)) return path;
    }
    return QString();
}

bool parseQtOptions(int argc, char** argv, QtOptions* out, QString* error)
{
    for (int i = 1; i < argc; ++i) {
        QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == "--help" || arg == "-h") { out->help = true; continue; }
        if (!arg.startsWith("--")) {
            *error = QStringLiteral("unknown argument: ") + arg;
            return false;
        }
        QString name = arg, value;
        const int eq = arg.indexOf('=');
        if (eq >= 0) { name = arg.left(eq); value = arg.mid(eq + 1); }
        else if (name == "--font-path" || name == "--font-size" || name == "--shell" ||
                 name == "--scrollback" || name == "--keyboard" || name == "--mouse") {
            if (i + 1 >= argc) { *error = name + " requires a value"; return false; }
            value = QString::fromLocal8Bit(argv[++i]);
        }
        if (name == "--font-path") out->fontPath = value;
        else if (name == "--font-size") {
            if (!parseInt(value, 8, 96, &out->fontSize)) {
                *error = "--font-size must be 8..96"; return false;
            }
        } else if (name == "--shell") out->shell = value;
        else if (name == "--scrollback") {
            if (!parseInt(value, 0, 1000000, &out->scrollback)) {
                *error = "--scrollback must be 0..1000000"; return false;
            }
        } else if (name == "--keyboard") out->keyboard = value;
        else if (name == "--mouse") out->mouse = value;
        else { *error = QStringLiteral("unknown option: ") + name; return false; }
    }
    return true;
}

void printQtUsage()
{
    QTextStream(stdout)
        << "Usage: fbterm-qt [options]\n"
        << "  --font-path=PATH      Unifont OTF path\n"
        << "  --font-size=N         pixel size 8..96 (default 32)\n"
        << "  --shell=CMD           PTY program (default /bin/login)\n"
        << "  --scrollback=N        history lines (default 5000)\n"
        << "  --keyboard=PATH       evdev keyboard (default auto)\n"
        << "  --mouse=PATH          evdev mouse (default auto)\n";
}
