#ifndef LVFB_QT_TERMINALWINDOW_H
#define LVFB_QT_TERMINALWINDOW_H

#include <qtermwidget.h>
class QLabel;

#include "ime.h"
#include "options.h"

class TerminalWindow : public QTermWidget {
    Q_OBJECT
public:
    TerminalWindow(const QtOptions& options, const QString& fontFamily,
                   QWidget* parent = nullptr);

    void setFontSize(int pixelSize);
    int fontSize() const { return fontSize_; }

    // Called from EvdevInput before the event reaches TerminalDisplay.
    bool handleImeKey(int key, Qt::KeyboardModifiers mods, const QString& text);
    void toggleImeMode();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onFinished();

private:
    void applyFont();
    void layoutImeLabel();
    void updateImeLabel();

    QtOptions options_;
    PinyinIme ime_;
    QLabel* imeLabel_ = nullptr;
    QString fontFamily_;
    int fontSize_ = 32;
};

#endif
