#include "app/log.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <strings.h>
#include <sys/time.h>

namespace lvfb {

namespace {
LogLevel g_level = LogLevel::Info;

const char* levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Error:
        return "E";
    case LogLevel::Warn:
        return "W";
    case LogLevel::Info:
        return "I";
    case LogLevel::Debug:
        return "D";
    }
    return "?";
}
} // namespace

void logSetLevel(LogLevel level)
{
    g_level = level;
}

LogLevel logGetLevel()
{
    return g_level;
}

bool logParseLevel(const char* text, LogLevel* out)
{
    if (text == nullptr || out == nullptr) {
        return false;
    }
    if (strcasecmp(text, "error") == 0) {
        *out = LogLevel::Error;
    } else if (strcasecmp(text, "warn") == 0 || strcasecmp(text, "warning") == 0) {
        *out = LogLevel::Warn;
    } else if (strcasecmp(text, "info") == 0) {
        *out = LogLevel::Info;
    } else if (strcasecmp(text, "debug") == 0) {
        *out = LogLevel::Debug;
    } else {
        return false;
    }
    return true;
}

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...)
{
    if (static_cast<int>(level) > static_cast<int>(g_level)) {
        return;
    }

    struct timeval tv;
    gettimeofday(&tv, nullptr);
    struct tm tmv;
    localtime_r(&tv.tv_sec, &tmv);

    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &tmv);

    const char* base = file;
    const char* slash = strrchr(file, '/');
    if (slash != nullptr) {
        base = slash + 1;
    }

    std::fprintf(stderr, "[%s.%03ld] %s %s:%d: ", timestamp,
                 static_cast<long>(tv.tv_usec / 1000), levelName(level), base, line);

    va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

} // namespace lvfb
