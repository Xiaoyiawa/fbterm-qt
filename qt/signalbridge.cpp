#include "signalbridge.h"

#include <QCoreApplication>
#include <QSocketNotifier>

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <unistd.h>

namespace {
int gWriteFd = -1;
}

void SignalBridge::handler(int)
{
    const char byte = 1;
    if (gWriteFd >= 0) {
        const ssize_t ignored = write(gWriteFd, &byte, 1);
        (void)ignored;
    }
}

SignalBridge::SignalBridge(QObject* parent)
    : QObject(parent)
{
    if (pipe(fds_) != 0) {
        return;
    }
    for (int i = 0; i < 2; ++i) {
        fcntl(fds_[i], F_SETFL, fcntl(fds_[i], F_GETFL, 0) | O_NONBLOCK);
        fcntl(fds_[i], F_SETFD, FD_CLOEXEC);
    }
    gWriteFd = fds_[1];

    struct sigaction action;
    action.sa_handler = &SignalBridge::handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    sigaction(SIGTERM, &action, nullptr);
    sigaction(SIGINT, &action, nullptr);
    sigaction(SIGHUP, &action, nullptr);

    notifier_ = new QSocketNotifier(fds_[0], QSocketNotifier::Read, this);
    connect(notifier_, &QSocketNotifier::activated, this,
            [this](int) { onActivated(); });
}

SignalBridge::~SignalBridge()
{
    gWriteFd = -1;
    signal(SIGTERM, SIG_DFL);
    signal(SIGINT, SIG_DFL);
    signal(SIGHUP, SIG_DFL);
    if (fds_[0] >= 0) close(fds_[0]);
    if (fds_[1] >= 0) close(fds_[1]);
}

void SignalBridge::onActivated()
{
    char buffer[64];
    while (read(fds_[0], buffer, sizeof(buffer)) > 0) {
    }
    QCoreApplication::quit();
}
