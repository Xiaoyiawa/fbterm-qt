#include <QApplication>
#include <QFontDatabase>
#include <QTimer>
#include <QTextStream>
#include <QStringList>

#include <unistd.h>

#include "consolemode.h"
#include "evdevinput.h"
#include "options.h"
#include "signalbridge.h"
#include "terminalwindow.h"

int main(int argc, char** argv)
{
    QtOptions options;
    QString error;
    if (!parseQtOptions(argc, argv, &options, &error)) {
        QTextStream(stderr) << "lvfbterm-qt: " << error << "\n";
        printQtUsage();
        return 2;
    }
    if (options.help) { printQtUsage(); return 0; }

    qputenv("QT_QPA_PLATFORM", "linuxfb");
    qputenv("QT_QPA_GENERIC_PLUGINS", "none");
    // Do not consume the SSH/console stdin as a keyboard. The terminal on the
    // TV must be driven by the physical USB keyboard via evdev.
    qputenv("QT_QPA_ENABLE_TERMINAL_KEYBOARD", "0");
    if (qEnvironmentVariableIsEmpty("XDG_RUNTIME_DIR")) qputenv("XDG_RUNTIME_DIR", "/tmp");

    ConsoleMode consoleMode;
    consoleMode.enterGraphics();

    QApplication app(argc, argv);
    SignalBridge signalBridge;

    const QString fontPath = resolveFontPath(options.fontPath);
    QString family = QApplication::font().family();
    if (!fontPath.isEmpty()) {
        const int id = QFontDatabase::addApplicationFont(fontPath);
        if (id >= 0) {
            const QStringList families = QFontDatabase::applicationFontFamilies(id);
            if (!families.isEmpty()) family = families.first();
        }
    }

    TerminalWindow window(options, family);
    window.showFullScreen();
    window.startShellProgram();
    QTimer::singleShot(0, &window, [&window]() { window.setFocus(); });

    EvdevInput input(&window, options);
    return app.exec();
}
