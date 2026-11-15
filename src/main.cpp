#include "app/app.h"
#include "app/options.h"

#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
    lvfb::Options options;
    std::string error;
    if (!lvfb::parseOptions(argc, argv, &options, &error)) {
        std::fprintf(stderr, "lvfbterm: %s\n\n", error.c_str());
        lvfb::printUsage(argv[0]);
        return 2;
    }

    if (options.showHelp) {
        lvfb::printUsage(argv[0]);
        return 0;
    }
    if (options.showVersion) {
        std::printf("lvfbterm 0.1.0\n");
        return 0;
    }

    lvfb::App app;
    return app.run(options);
}
