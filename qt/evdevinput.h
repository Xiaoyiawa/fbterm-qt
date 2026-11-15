#ifndef LVFB_QT_EVDEVINPUT_H
#define LVFB_QT_EVDEVINPUT_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>
#include <Qt>

#include "options.h"

class QSocketNotifier;
class QTermWidget;

// Reads /dev/input/event* directly in the Qt main thread and synthesizes
// QKeyEvent/QWheelEvent for QTermWidget. This keeps hotplug control in our
// hands and avoids Qt's one-shot evdev generic plugin.
class EvdevInput : public QObject {
    Q_OBJECT
public:
    EvdevInput(QTermWidget* terminal, const QtOptions& options,
               QObject* parent = nullptr);
    ~EvdevInput();

private slots:
    void scanKeyboards();
    void scanMice();

private:
    struct Device {
        int fd = -1;
        bool keyboard = false;
        bool mouse = false;
        QString path;
        QSocketNotifier* notifier = nullptr;
    };

    bool openDevice(const QString& path, bool wantKeyboard);
    void readDevice(int fd);
    void removeDevice(int fd);
    bool hasKeyboard() const;
    bool isOpen(const QString& path) const;
    QWidget* eventTarget() const;

    void processKey(int code, int value);
    void processWheel(int delta);
    void sendKey(int key, Qt::KeyboardModifiers mods, const QString& text);
    Qt::KeyboardModifiers modifiers() const;

    QTermWidget* terminal_ = nullptr;
    QtOptions options_;
    QVector<Device> devices_;
    QTimer* keyboardTimer_ = nullptr;
    QTimer* mouseTimer_ = nullptr;

    bool ctrl_ = false;
    bool alt_ = false;
    bool shift_ = false;
    bool caps_ = false;
};

#endif
