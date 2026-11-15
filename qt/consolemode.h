#ifndef LVFB_QT_CONSOLEMODE_H
#define LVFB_QT_CONSOLEMODE_H

class ConsoleMode {
public:
    ConsoleMode() = default;
    ~ConsoleMode();
    void enterGraphics();
private:
    int ttyFd_ = -1;
    bool graphics_ = false;
};

#endif
