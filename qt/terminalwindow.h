#ifndef LVFB_QT_TERMINALWINDOW_H
#define LVFB_QT_TERMINALWINDOW_H

#include <qtermwidget.h>
#include "options.h"

class TerminalWindow : public QTermWidget {
    Q_OBJECT
public:
    TerminalWindow(const QtOptions& options, const QString& fontFamily,
                   QWidget* parent = nullptr);

    void setFontSize(int pixelSize);
    int fontSize() const { return fontSize_; }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onFinished();

private:
    void applyFont();

    QtOptions options_;
    QString fontFamily_;
    int fontSize_ = 32;
    bool loginMode_ = true;
};

#endif
