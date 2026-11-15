#ifndef LVFB_APP_OPTIONS_H
#define LVFB_APP_OPTIONS_H

#include "app/log.h"

#include <string>
#include <vector>

namespace lvfb {

struct Options {
    std::string fb = "/dev/fb0";
    int fontSize = 32;
    std::vector<std::string> fontPaths;
    int scrollback = 5000;
    std::string shell = "/bin/login";
    std::vector<std::string> keyboards;
    std::vector<std::string> mice;
    LogLevel logLevel = LogLevel::Info;
    bool showHelp = false;
    bool showVersion = false;
};

// Returns false on error and fills |error|.
bool parseOptions(int argc, char** argv, Options* out, std::string* error);

void printUsage(const char* argv0);

} // namespace lvfb

#endif // LVFB_APP_OPTIONS_H
