#include "app/options.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace lvfb {

namespace {

const char* kVersion = "0.1.0";

bool parseInt(const std::string& text, int minValue, int maxValue, int* out)
{
    if (text.empty()) {
        return false;
    }
    errno = 0;
    char* end = nullptr;
    long value = std::strtol(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || *end != '\0') {
        return false;
    }
    if (value < minValue || value > maxValue) {
        return false;
    }
    *out = static_cast<int>(value);
    return true;
}

bool needsValue(const std::string& name)
{
    return name == "--fb" || name == "--font-size" || name == "--font-path" ||
           name == "--scrollback" || name == "--shell" || name == "--keyboard" ||
           name == "--mouse" || name == "--log-level";
}

bool applyOption(const std::string& name, const std::string& value, Options* out,
                 std::string* error)
{
    if (name == "--fb") {
        if (value.empty()) {
            *error = "--fb requires a non-empty device path";
            return false;
        }
        out->fb = value;
        return true;
    }
    if (name == "--font-size") {
        int size = 0;
        if (!parseInt(value, 8, 96, &size)) {
            *error = "--font-size must be an integer in [8, 96]";
            return false;
        }
        out->fontSize = size;
        return true;
    }
    if (name == "--font-path") {
        if (value.empty()) {
            *error = "--font-path requires a path";
            return false;
        }
        out->fontPaths.push_back(value);
        return true;
    }
    if (name == "--scrollback") {
        int lines = 0;
        if (!parseInt(value, 0, 1000000, &lines)) {
            *error = "--scrollback must be an integer in [0, 1000000]";
            return false;
        }
        out->scrollback = lines;
        return true;
    }
    if (name == "--shell") {
        if (value.empty()) {
            *error = "--shell requires a command";
            return false;
        }
        out->shell = value;
        return true;
    }
    if (name == "--keyboard") {
        if (value.empty()) {
            *error = "--keyboard requires a device path";
            return false;
        }
        out->keyboards.push_back(value);
        return true;
    }
    if (name == "--mouse") {
        if (value.empty()) {
            *error = "--mouse requires a device path";
            return false;
        }
        out->mice.push_back(value);
        return true;
    }
    if (name == "--log-level") {
        LogLevel level = LogLevel::Info;
        if (!logParseLevel(value.c_str(), &level)) {
            *error = "--log-level must be one of error, warn, info, debug";
            return false;
        }
        out->logLevel = level;
        return true;
    }

    *error = "unknown option: " + name;
    return false;
}

} // namespace

bool parseOptions(int argc, char** argv, Options* out, std::string* error)
{
    if (out == nullptr || error == nullptr) {
        return false;
    }

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h") {
            out->showHelp = true;
            continue;
        }
        if (arg == "--version") {
            out->showVersion = true;
            continue;
        }
        if (arg == "--") {
            if (i + 1 < argc) {
                *error = "unexpected positional argument: " + std::string(argv[i + 1]);
                return false;
            }
            break;
        }
        if (arg.size() < 3 || arg[0] != '-' || arg[1] != '-') {
            *error = "unexpected argument: " + arg;
            return false;
        }

        std::string name;
        std::string value;
        const std::string::size_type eq = arg.find('=');
        if (eq != std::string::npos) {
            name = arg.substr(0, eq);
            value = arg.substr(eq + 1);
        } else {
            name = arg;
            if (needsValue(name)) {
                if (i + 1 >= argc) {
                    *error = name + " requires a value";
                    return false;
                }
                value = argv[++i];
            }
        }

        if (!applyOption(name, value, out, error)) {
            return false;
        }
    }

    return true;
}

void printUsage(const char* argv0)
{
    const char* name = argv0 != nullptr ? argv0 : "lvfbterm";
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "Framebuffer terminal: LVGL + FreeType + libvterm + PTY.\n"
        "\n"
        "Options:\n"
        "  --fb=PATH             framebuffer device (default: /dev/fb0)\n"
        "  --font-size=N         font pixel height, 8..96 (default: 32)\n"
        "  --font-path=PATH      font file, repeatable; first is primary, rest fallbacks\n"
        "  --scrollback=N        scrollback lines (default: 5000)\n"
        "  --shell=CMD           program in the PTY (default: /bin/login)\n"
        "  --keyboard=PATH       evdev keyboard, repeatable (default: auto scan)\n"
        "  --mouse=PATH          evdev mouse, repeatable (default: auto scan)\n"
        "  --log-level=LEVEL     error|warn|info|debug (default: info)\n"
        "  --help                show this help\n"
        "  --version             show version\n"
        "\n"
        "Hotkeys:\n"
        "  Ctrl+Alt+Q            quit\n"
        "  Ctrl+wheel            zoom font (+/- 2 px)\n"
        "  wheel                 scrollback; arrows in alternate screen\n",
        name);
    std::printf("Version: %s\n", kVersion);
}

} // namespace lvfb
