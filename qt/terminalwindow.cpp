#include "terminalwindow.h"

#include <QCoreApplication>
#include <QFont>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QApplication>
#include <QLabel>
#include <QResizeEvent>

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

    setShellProgram(program);
    if (!parts.isEmpty()) setArgs(parts);
    setHistorySize(options_.scrollback);

    // Use xterm F1-F12 sequences (the "linux" keytab sends ESC[[A..E for
    // F1-F5, which xterm-256color cannot parse and leaves a trailing 'B').
    // QTermWidget resolves keytab names under KB_LAYOUT_DIR; it does not
    // accept a path, so build.sh installs fbterm-qt.keytab there.
    const QString systemKeytab =
        QStringLiteral("/usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab");
    setKeyBindings(QFileInfo::exists(systemKeytab) ? QStringLiteral("fbterm-qt")
                                                   : QStringLiteral("default"));

    // QTermWidget inherits the parent environment; make the terminal type
    // explicit instead of relying on the service/session defaults.
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("TERM"), QStringLiteral("xterm-256color"));
    env.insert(QStringLiteral("COLORTERM"), QStringLiteral("truecolor"));
    setEnvironment(env.toStringList());

    const QStringList schemes = {
        QStringLiteral("./share/color-schemes/Campbell.colorscheme"),
        QStringLiteral("/root/fbterm-qt/share/color-schemes/Campbell.colorscheme"),
        QStringLiteral("/usr/share/qtermwidget5/color-schemes/Campbell.colorscheme"),
    };
    QString scheme;
    for (const QString& path : schemes) {
        if (QFileInfo::exists(path)) { scheme = path; break; }
    }
    setColorScheme(scheme.isEmpty() ? QStringLiteral("Linux") : scheme);
    setScrollBarPosition(QTermWidget::NoScrollBar);
    setKeyboardCursorShape(Konsole::Emulation::KeyboardCursorShape::BlockCursor);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::BlankCursor);
    setWorkingDirectory(QStringLiteral("/root"));

    applyFont();

    if (!options_.disableIme) ime_.init();
    imeLabel_ = new QLabel(this);
    imeLabel_->setAttribute(Qt::WA_TransparentForMouseEvents);
    imeLabel_->setWordWrap(true);
    imeLabel_->setStyleSheet(
        "QLabel { background-color: #0C0C0C; color: #CCCCCC;"
        " border-top: 1px solid #3A96DD; padding: 4px 8px; }");
    QFont imeFont = font();
    imeFont.setPixelSize(22);
    imeLabel_->setFont(imeFont);
    imeLabel_->hide();

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
    // The PTY child (login/shell) is gone. Do not clear/restart the widget:
    // clear() used to leave a white screen, and the requested behaviour is to
    // leave the terminal directly after `exit`.
    QCoreApplication::quit();
}

bool TerminalWindow::handleImeKey(int key, Qt::KeyboardModifiers mods,
                                  const QString& text)
{
    if (!ime_.enabled()) return false;
    QString commit;
    if (!ime_.handleKey(key, mods, text, &commit)) return false;
    if (!commit.isEmpty()) sendText(commit);
    updateImeLabel();
    return true;
}

void TerminalWindow::toggleImeMode()
{
    if (!ime_.enabled()) return;
    ime_.toggleMode();
    updateImeLabel();
}

void TerminalWindow::layoutImeLabel()
{
    if (imeLabel_ == nullptr || !imeLabel_->isVisible()) return;
    imeLabel_->setFixedWidth(width());
    imeLabel_->adjustSize();
    const int h = qMin(imeLabel_->height(), qMax(40, height() / 3));
    imeLabel_->setGeometry(0, height() - h, width(), h);
    imeLabel_->raise();
}

void TerminalWindow::updateImeLabel()
{
    if (imeLabel_ == nullptr) return;
    const QString text = ime_.display();
    if (text.isEmpty()) {
        imeLabel_->hide();
        return;
    }
    imeLabel_->setText(text);
    imeLabel_->show();
    layoutImeLabel();
}

void TerminalWindow::resizeEvent(QResizeEvent* event)
{
    QTermWidget::resizeEvent(event);
    layoutImeLabel();
}
