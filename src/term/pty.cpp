#include "term/pty.h"

#include "app/log.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pty.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

namespace lvfb {

namespace {

void childResetSignals()
{
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, nullptr);

    std::signal(SIGPIPE, SIG_DFL);
    std::signal(SIGCHLD, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGHUP, SIG_DFL);
    std::signal(SIGQUIT, SIG_DFL);
}

} // namespace

bool splitCommand(const std::string& command, std::vector<std::string>* out,
                  std::string* error)
{
    if (out == nullptr || error == nullptr) {
        return false;
    }
    out->clear();

    std::string current;
    bool inSingle = false;
    bool inDouble = false;
    bool escaped = false;

    for (size_t i = 0; i < command.size(); ++i) {
        const char ch = command[i];
        if (escaped) {
            current.push_back(ch);
            escaped = false;
            continue;
        }
        if (inSingle) {
            if (ch == '\'') {
                inSingle = false;
            } else {
                current.push_back(ch);
            }
            continue;
        }
        if (inDouble) {
            if (ch == '"') {
                inDouble = false;
            } else if (ch == '\\') {
                escaped = true;
            } else {
                current.push_back(ch);
            }
            continue;
        }
        if (ch == '\\') {
            escaped = true;
        } else if (ch == '\'') {
            inSingle = true;
        } else if (ch == '"') {
            inDouble = true;
        } else if (ch == ' ' || ch == '\t' || ch == '\n') {
            if (!current.empty()) {
                out->push_back(current);
                current.clear();
            }
        } else {
            current.push_back(ch);
        }
    }

    if (escaped || inSingle || inDouble) {
        *error = "unterminated quote or escape in --shell command";
        return false;
    }
    if (!current.empty()) {
        out->push_back(current);
    }
    if (out->empty()) {
        *error = "empty --shell command";
        return false;
    }
    return true;
}

Pty::Pty() = default;

Pty::~Pty()
{
    stop();
}

bool Pty::start(const std::string& command, int rows, int cols, std::string* error)
{
    if (rows <= 0) {
        rows = 1;
    }
    if (cols <= 0) {
        cols = 1;
    }

    std::vector<std::string> args;
    if (!splitCommand(command, &args, error)) {
        return false;
    }

    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);

    int master = -1;
    int slave = -1;
    if (openpty(&master, &slave, nullptr, nullptr, &ws) != 0) {
        if (error != nullptr) {
            *error = std::string("openpty failed: ") + std::strerror(errno);
        }
        return false;
    }

    const pid_t pid = fork();
    if (pid < 0) {
        if (error != nullptr) {
            *error = std::string("fork failed: ") + std::strerror(errno);
        }
        close(master);
        close(slave);
        return false;
    }

    if (pid == 0) {
        close(master);
        if (setsid() < 0) {
            _exit(126);
        }
        ioctl(slave, TIOCSCTTY, 0);

        if (dup2(slave, STDIN_FILENO) < 0 || dup2(slave, STDOUT_FILENO) < 0 ||
            dup2(slave, STDERR_FILENO) < 0) {
            _exit(126);
        }
        if (slave > STDERR_FILENO) {
            close(slave);
        }

        childResetSignals();

        setenv("TERM", "xterm-256color", 1);
        setenv("COLORTERM", "truecolor", 1);
        if (getenv("LANG") == nullptr && getenv("LC_ALL") == nullptr) {
            setenv("LANG", "C.UTF-8", 1);
        }

        std::vector<char*> argv;
        argv.reserve(args.size() + 1);
        for (size_t i = 0; i < args.size(); ++i) {
            argv.push_back(const_cast<char*>(args[i].c_str()));
        }
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        const int saved = errno;
        std::fprintf(stderr, "lvfbterm: exec %s: %s\r\n", argv[0], std::strerror(saved));
        _exit(saved == ENOENT ? 127 : 126);
    }

    close(slave);

    const int flags = fcntl(master, F_GETFL, 0);
    if (flags >= 0) {
        fcntl(master, F_SETFL, flags | O_NONBLOCK);
    }
    fcntl(master, F_SETFD, FD_CLOEXEC);

    master_ = master;
    pid_ = pid;
    running_ = true;
    command_ = command;
    pending_.clear();

    LVFB_LOGI("pty started: pid=%d rows=%d cols=%d command=%s", static_cast<int>(pid),
              rows, cols, command.c_str());
    return true;
}

void Pty::stop()
{
    if (master_ >= 0) {
        close(master_);
        master_ = -1;
    }

    if (pid_ > 0 && running_) {
        kill(-pid_, SIGHUP);
        for (int i = 0; i < 20; ++i) {
            int status = 0;
            const pid_t ret = waitpid(pid_, &status, WNOHANG);
            if (ret == pid_ || (ret < 0 && errno == ECHILD)) {
                running_ = false;
                break;
            }
            usleep(10000);
        }
        if (running_) {
            kill(-pid_, SIGTERM);
            for (int i = 0; i < 20; ++i) {
                int status = 0;
                const pid_t ret = waitpid(pid_, &status, WNOHANG);
                if (ret == pid_ || (ret < 0 && errno == ECHILD)) {
                    running_ = false;
                    break;
                }
                usleep(10000);
            }
        }
        if (running_) {
            kill(-pid_, SIGKILL);
            int status = 0;
            waitpid(pid_, &status, 0);
        }
    }

    running_ = false;
    pid_ = -1;
    pending_.clear();
}

bool Pty::readSome(std::string* out)
{
    if (master_ < 0) {
        return false;
    }

    char buffer[65536];
    for (;;) {
        const ssize_t n = read(master_, buffer, sizeof(buffer));
        if (n > 0) {
            out->append(buffer, static_cast<size_t>(n));
            return true;
        }
        if (n == 0) {
            return false;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        return false;
    }
}

bool Pty::flushWrites()
{
    while (master_ >= 0 && !pending_.empty()) {
        const ssize_t n = write(master_, pending_.data(), pending_.size());
        if (n > 0) {
            pending_.erase(0, static_cast<size_t>(n));
            continue;
        }
        if (n < 0 && errno == EINTR) {
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return true;
        }
        return false;
    }
    return true;
}

bool Pty::writeAll(const char* data, size_t len)
{
    if (data == nullptr || len == 0) {
        return true;
    }
    pending_.append(data, len);
    return flushWrites();
}

bool Pty::resize(int rows, int cols)
{
    if (master_ < 0 || rows <= 0 || cols <= 0) {
        return false;
    }
    struct winsize ws;
    std::memset(&ws, 0, sizeof(ws));
    ws.ws_row = static_cast<unsigned short>(rows);
    ws.ws_col = static_cast<unsigned short>(cols);
    return ioctl(master_, TIOCSWINSZ, &ws) == 0;
}

bool Pty::reap(int* status)
{
    if (pid_ <= 0) {
        return false;
    }
    int localStatus = 0;
    const pid_t ret = waitpid(pid_, &localStatus, WNOHANG);
    if (ret == pid_) {
        running_ = false;
        if (status != nullptr) {
            *status = localStatus;
        }
        return true;
    }
    if (ret < 0 && errno == ECHILD) {
        running_ = false;
        if (status != nullptr) {
            *status = 0;
        }
        return true;
    }
    return false;
}

} // namespace lvfb
