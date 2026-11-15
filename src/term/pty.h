#ifndef LVFB_TERM_PTY_H
#define LVFB_TERM_PTY_H

#include <string>
#include <sys/types.h>
#include <vector>

namespace lvfb {

class Pty {
public:
    Pty();
    ~Pty();

    Pty(const Pty&) = delete;
    Pty& operator=(const Pty&) = delete;

    bool start(const std::string& command, int rows, int cols, std::string* error);
    void stop();

    bool running() const { return running_; }
    int masterFd() const { return master_; }
    pid_t pid() const { return pid_; }
    const std::string& command() const { return command_; }

    // Reads one chunk. Returns false when the slave side is gone.
    bool readSome(std::string* out);

    // Queues bytes for the PTY. Returns false on a hard error.
    bool writeAll(const char* data, size_t len);
    bool flushWrites();
    bool hasPendingWrites() const { return !pending_.empty(); }

    bool resize(int rows, int cols);

    // Returns true when the child exited; |status| is waitpid status.
    bool reap(int* status);

private:
    int master_ = -1;
    pid_t pid_ = -1;
    bool running_ = false;
    std::string command_;
    std::string pending_;
};

bool splitCommand(const std::string& command, std::vector<std::string>* out,
                  std::string* error);

} // namespace lvfb

#endif // LVFB_TERM_PTY_H
