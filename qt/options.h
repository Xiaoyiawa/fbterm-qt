#ifndef LVFB_QT_OPTIONS_H
#define LVFB_QT_OPTIONS_H

#include <QString>
#include <QStringList>

struct QtOptions {
    QStringList fontPaths;
    int fontSize = 32;
    QString shell = "/bin/login";
    int scrollback = 5000;
    QString keyboard;
    QString mouse;
    bool help = false;
    bool disableIme = false;
};

bool parseQtOptions(int argc, char** argv, QtOptions* out, QString* error);
QStringList splitCommandLine(const QString& command);
QStringList resolveFontPaths(const QStringList& explicitPaths);
void printQtUsage();

#endif
