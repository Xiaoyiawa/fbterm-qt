#include "input/input.h"

#include "app/log.h"

#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <unistd.h>

namespace lvfb {

namespace {

bool testBit(const unsigned long* bits, int bit)
{
    return (bits[bit / (8 * static_cast<int>(sizeof(unsigned long)))] &
            (1ul << (bit % (8 * static_cast<int>(sizeof(unsigned long)))))) != 0;
}

std::string deviceName(int fd)
{
    char name[256];
    std::memset(name, 0, sizeof(name));
    if (ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) >= 0 && name[0] != '\0') {
        return std::string(name);
    }
    return std::string("unknown");
}

} // namespace

InputManager::InputManager() = default;

InputManager::~InputManager()
{
    stop();
}

bool InputManager::isOpen(const std::string& path) const
{
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].path == path) {
            return true;
        }
    }
    return false;
}

bool InputManager::openDevice(const std::string& path)
{
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        if (isOpen(path)) {
            return true;
        }
    }

    const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return false;
    }

    unsigned long evBits[(EV_MAX + 8 * static_cast<int>(sizeof(unsigned long))) /
                         (8 * static_cast<int>(sizeof(unsigned long)))] = {};
    if (ioctl(fd, EVIOCGBIT(0, sizeof(evBits)), evBits) < 0) {
        close(fd);
        return false;
    }

    bool hasKeys = false;
    bool hasWheel = false;
    bool hasMouseRel = false;

    if (testBit(evBits, EV_KEY)) {
        unsigned long keyBits[(KEY_MAX + 8 * static_cast<int>(sizeof(unsigned long))) /
                              (8 * static_cast<int>(sizeof(unsigned long)))] = {};
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits) >= 0) {
            hasKeys = testBit(keyBits, KEY_A) && testBit(keyBits, KEY_ENTER) &&
                      testBit(keyBits, KEY_SPACE);
        }
    }

    if (testBit(evBits, EV_REL)) {
        unsigned long relBits[(REL_MAX + 8 * static_cast<int>(sizeof(unsigned long))) /
                              (8 * static_cast<int>(sizeof(unsigned long)))] = {};
        if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relBits)), relBits) >= 0) {
            hasWheel = testBit(relBits, REL_WHEEL);
            hasMouseRel = hasWheel || testBit(relBits, REL_X) || testBit(relBits, REL_Y);
        }
    }

    if (!hasKeys && !hasMouseRel) {
        close(fd);
        return false;
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        LVFB_LOGW("EVIOCGRAB failed for %s: %s", path.c_str(), std::strerror(errno));
    }

    Device dev;
    dev.fd = fd;
    dev.keyboard = hasKeys;
    dev.mouse = hasMouseRel;
    dev.path = path;

    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        if (isOpen(path)) {
            ioctl(fd, EVIOCGRAB, 0);
            close(fd);
            return true;
        }
        devices_.push_back(dev);
    }

    LVFB_LOGI("input device: %s (%s)%s%s%s", path.c_str(), deviceName(fd).c_str(),
              hasKeys ? " keyboard" : "", hasMouseRel ? " mouse" : "",
              hasWheel ? " wheel" : "");
    return true;
}

void InputManager::scan()
{
    if (!autoScan_) {
        return;
    }


    for (int i = 0; i < 64; ++i) {
        char path[64];
        std::snprintf(path, sizeof(path), "/dev/input/event%d", i);
        if (access(path, R_OK) != 0) {
            continue;
        }
        openDevice(path);
    }

    lastScan_ = std::time(nullptr);
}

void InputManager::removeDevice(size_t index)
{
    int fd = -1;
    {
        std::lock_guard<std::mutex> lock(devicesMutex_);
        if (index >= devices_.size()) {
            return;
        }
        fd = devices_[index].fd;
        devices_.erase(devices_.begin() + static_cast<std::ptrdiff_t>(index));
    }
    if (fd >= 0) {
        LVFB_LOGW("input device removed");
        ioctl(fd, EVIOCGRAB, 0);
        close(fd);
    }
}

void InputManager::threadLoop()
{
    while (running_.load()) {
        if (autoScan_) {
            const time_t now = std::time(nullptr);
            if (now - lastScan_ >= 2) {
                scan();
            }
        }

        std::vector<pollfd> fds;
        {
            std::lock_guard<std::mutex> lock(devicesMutex_);
            for (size_t i = 0; i < devices_.size(); ++i) {
                if (devices_[i].fd < 0) {
                    continue;
                }
                pollfd pfd;
                pfd.fd = devices_[i].fd;
                pfd.events = POLLIN;
                pfd.revents = 0;
                fds.push_back(pfd);
            }
        }

        const int ret = poll(fds.data(), static_cast<nfds_t>(fds.size()), 100);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            LVFB_LOGE("input poll failed: %s", std::strerror(errno));
            break;
        }
        if (ret == 0) {
            continue;
        }

        std::vector<InputEvent> events;
        for (size_t i = 0; i < fds.size(); ++i) {
            if (fds[i].revents == 0) {
                continue;
            }

            int fd = fds[i].fd;
            bool keyboard = false;
            bool mouse = false;
            size_t index = devices_.size();
            {
                std::lock_guard<std::mutex> lock(devicesMutex_);
                for (size_t j = 0; j < devices_.size(); ++j) {
                    if (devices_[j].fd == fd) {
                        index = j;
                        keyboard = devices_[j].keyboard;
                        mouse = devices_[j].mouse;
                        break;
                    }
                }
            }
            if (index >= devices_.size()) {
                continue;
            }

            if ((fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
                removeDevice(index);
                continue;
            }

            for (;;) {
                struct input_event evs[64];
                const ssize_t n = read(fd, evs, sizeof(evs));
                if (n < 0) {
                    if (errno == EINTR) {
                        continue;
                    }
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    }
                    removeDevice(index);
                    break;
                }
                if (n == 0) {
                    removeDevice(index);
                    break;
                }

                const size_t count = static_cast<size_t>(n) / sizeof(struct input_event);
                for (size_t k = 0; k < count; ++k) {
                    const struct input_event& ev = evs[k];
                    if (ev.type == EV_KEY && keyboard && ev.code < KEY_MAX) {
                        InputEvent out;
                        out.type = InputEventType::Key;
                        out.code = ev.code;
                        out.value = ev.value;
                        events.push_back(out);
                    } else if (ev.type == EV_REL && mouse && ev.code == REL_WHEEL) {
                        InputEvent out;
                        out.type = InputEventType::Wheel;
                        out.wheel = ev.value;
                        events.push_back(out);
                    }
                }
            }
        }

        if (!events.empty()) {
            std::lock_guard<std::mutex> lock(queueMutex_);
            for (size_t i = 0; i < events.size() && queue_.size() < 65536; ++i) {
                queue_.push_back(events[i]);
            }
        }
    }
}

bool InputManager::init(const std::vector<std::string>& keyboards,
                        const std::vector<std::string>& mice, std::string* error)
{
    (void)error;
    stop();

    if (!keyboards.empty() || !mice.empty()) {
        autoScan_ = false;
        for (size_t i = 0; i < keyboards.size(); ++i) {
            openDevice(keyboards[i]);
        }
        for (size_t i = 0; i < mice.size(); ++i) {
            openDevice(mice[i]);
        }
    } else {
        autoScan_ = true;
        scan();
    }

    if (!hasKeyboard()) {
        LVFB_LOGW("no keyboard found; plug in a USB keyboard (will rescan)");
    }
    if (!hasMouse()) {
        LVFB_LOGW("no mouse wheel found; Ctrl+wheel zoom and scrollback disabled");
    }

    running_.store(true);
    thread_ = std::thread(&InputManager::threadLoop, this);
    return true;
}

void InputManager::stop()
{
    if (running_.exchange(false)) {
        if (thread_.joinable()) {
            thread_.join();
        }
    }
    closeAll();
}

void InputManager::closeAll()
{
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].fd >= 0) {
            ioctl(devices_[i].fd, EVIOCGRAB, 0);
            close(devices_[i].fd);
        }
    }
    devices_.clear();
}

void InputManager::drainEvents(std::vector<InputEvent>* events)
{
    if (events == nullptr) {
        return;
    }
    std::deque<InputEvent> local;
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        local.swap(queue_);
    }
    events->assign(local.begin(), local.end());
}

bool InputManager::hasKeyboard() const
{
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].keyboard) {
            return true;
        }
    }
    return false;
}

bool InputManager::hasMouse() const
{
    std::lock_guard<std::mutex> lock(devicesMutex_);
    for (size_t i = 0; i < devices_.size(); ++i) {
        if (devices_[i].mouse) {
            return true;
        }
    }
    return false;
}

} // namespace lvfb
