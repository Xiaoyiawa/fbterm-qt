#ifndef LVFB_QT_SIGNALBRIDGE_H
#define LVFB_QT_SIGNALBRIDGE_H

#include <QObject>

class QSocketNotifier;

// Bridges SIGTERM/SIGINT/SIGHUP into the Qt event loop so that
// ConsoleMode's destructor can restore KD_TEXT on `systemctl stop`.
class SignalBridge : public QObject {
    Q_OBJECT
public:
    explicit SignalBridge(QObject* parent = nullptr);
    ~SignalBridge();

private:
    static void handler(int signo);
    void onActivated();

    int fds_[2] = {-1, -1};
    QSocketNotifier* notifier_ = nullptr;
};

#endif
