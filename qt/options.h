#ifndef LVFB_QT_OPTIONS_H
#define LVFB_QT_OPTIONS_H

#include <QString>
#include <QStringList>

struct QtOptions {
    QString fontPath;
    int fontSize = 32;
    QString shell = "/bin/login";
    int scrollback = 5000;
    QString keyboard;
    QString mouse;
    bool help = false;
};

bool parseQtOptions(int argc, char** argv, QtOptions* out, QString* error);
QStringList splitCommandLine(const QString& command);
QString resolveFontPath(const QString& explicitPath);
void printQtUsage();

#endif
