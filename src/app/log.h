#ifndef LVFB_APP_LOG_H
#define LVFB_APP_LOG_H

namespace lvfb {

enum class LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
};

void logSetLevel(LogLevel level);
LogLevel logGetLevel();
bool logParseLevel(const char* text, LogLevel* out);

void logMessage(LogLevel level, const char* file, int line, const char* fmt, ...)
    __attribute__((format(printf, 4, 5)));

} // namespace lvfb

#define LVFB_LOGE(...) ::lvfb::logMessage(::lvfb::LogLevel::Error, __FILE__, __LINE__, __VA_ARGS__)
#define LVFB_LOGW(...) ::lvfb::logMessage(::lvfb::LogLevel::Warn, __FILE__, __LINE__, __VA_ARGS__)
#define LVFB_LOGI(...) ::lvfb::logMessage(::lvfb::LogLevel::Info, __FILE__, __LINE__, __VA_ARGS__)
#define LVFB_LOGD(...) ::lvfb::logMessage(::lvfb::LogLevel::Debug, __FILE__, __LINE__, __VA_ARGS__)

#endif // LVFB_APP_LOG_H
