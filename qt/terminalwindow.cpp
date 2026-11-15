#include "terminalwindow.h"

#include <QCoreApplication>
#include <QFont>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>

TerminalWindow::TerminalWindow(const QtOptions& options, const QString& fontFamily,
                               QWidget* parent)
    : QTermWidget(0, parent)
    , options_(options)
    , fontFamily_(fontFamily)
    , fontSize_(options.fontSize)
{
    QStringList parts = splitCommandLine(options_.shell);
    if (parts.isEmpty()) parts << QStringLiteral("/bin/login");
    const QString program = parts.takeFirst();
    loginMode_ = program.endsWith(QStringLiteral("login"));

    setShellProgram(program);
    if (!parts.isEmpty()) setArgs(parts);
    setHistorySize(options_.scrollback);
    setKeyBindings(QStringLiteral("linux"));
    setColorScheme(QStringLiteral("Linux"));
    setScrollBarPosition(QTermWidget::NoScrollBar);
    setKeyboardCursorShape(Konsole::Emulation::KeyboardCursorShape::BlockCursor);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);
    setWorkingDirectory(QStringLiteral("/root"));

    applyFont();
    connect(this, &QTermWidget::finished, this, &TerminalWindow::onFinished);
    qApp->installEventFilter(this);
}

void TerminalWindow::applyFont()
{
    QFont font(fontFamily_);
    font.setPixelSize(fontSize_);
    font.setFixedPitch(true);
    font.setStyleHint(QFont::TypeWriter);
    font.setStyleStrategy((fontSize_ % 16) == 0 ? QFont::NoAntialias
                                                : QFont::PreferAntialias);
    setTerminalFont(font);
    update();
}

void TerminalWindow::setFontSize(int pixelSize)
{
    if (pixelSize < 8) pixelSize = 8;
    if (pixelSize > 96) pixelSize = 96;
    if (pixelSize == fontSize_) return;
    fontSize_ = pixelSize;
    applyFont();
}

bool TerminalWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* key = static_cast<QKeyEvent*>(event);
        const Qt::KeyboardModifiers mods = key->modifiers();
        if (key->key() == Qt::Key_Q &&
            (mods & (Qt::ControlModifier | Qt::AltModifier)) ==
                (Qt::ControlModifier | Qt::AltModifier)) {
            QCoreApplication::quit();
            return true;
        }
    } else if (event->type() == QEvent::Wheel) {
        QWheelEvent* wheel = static_cast<QWheelEvent*>(event);
        if ((wheel->modifiers() & Qt::ControlModifier) != 0) {
            const int delta = wheel->angleDelta().y();
            if (delta != 0) {
                setFontSize(fontSize_ + (delta > 0 ? 2 : -2));
                return true;
            }
        }
    }
    return QTermWidget::eventFilter(watched, event);
}

void TerminalWindow::keyPressEvent(QKeyEvent* event)
{
    const Qt::KeyboardModifiers mods = event->modifiers();
    if (event->key() == Qt::Key_Q &&
        (mods & (Qt::ControlModifier | Qt::AltModifier)) ==
            (Qt::ControlModifier | Qt::AltModifier)) {
        QCoreApplication::quit();
        return;
    }
    QTermWidget::keyPressEvent(event);
}

void TerminalWindow::wheelEvent(QWheelEvent* event)
{
    if ((event->modifiers() & Qt::ControlModifier) != 0) {
        const int delta = event->angleDelta().y();
        if (delta != 0) {
            setFontSize(fontSize_ + (delta > 0 ? 2 : -2));
            event->accept();
            return;
        }
    }
    QTermWidget::wheelEvent(event);
}

void TerminalWindow::onFinished()
{
    if (loginMode_) {
        clear();
        startShellProgram();
    } else {
        QCoreApplication::quit();
    }
}
