#ifndef LVFB_INPUT_INPUT_H
#define LVFB_INPUT_INPUT_H

#include <atomic>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace lvfb {

enum class InputEventType {
    Key,
    Wheel,
};

struct InputEvent {
    InputEventType type = InputEventType::Key;
    int code = 0;  // KEY_* for keys
    int value = 0; // 0 release, 1 press, 2 repeat
    int wheel = 0; // +1 up, -1 down
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    InputManager(const InputManager&) = delete;
    InputManager& operator=(const InputManager&) = delete;

    bool init(const std::vector<std::string>& keyboards,
              const std::vector<std::string>& mice, std::string* error);
    void stop();

    // Moves all queued events into |events|. Safe to call from the main thread
    // while the reader thread keeps draining evdev.
    void drainEvents(std::vector<InputEvent>* events);

    bool hasKeyboard() const;
    bool hasMouse() const;

private:
    struct Device {
        int fd = -1;
        bool keyboard = false;
        bool mouse = false;
        std::string path;
    };

    void threadLoop();
    bool openDevice(const std::string& path);
    void scan();
    void removeDevice(size_t index);
    bool isOpen(const std::string& path) const;
    void closeAll();

    mutable std::mutex devicesMutex_;
    std::vector<Device> devices_;
    bool autoScan_ = true;
    time_t lastScan_ = 0;

    std::atomic<bool> running_{false};
    std::thread thread_;

    std::mutex queueMutex_;
    std::deque<InputEvent> queue_;
};

} // namespace lvfb

#endif // LVFB_INPUT_INPUT_H
