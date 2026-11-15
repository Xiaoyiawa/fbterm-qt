QT += widgets
CONFIG += c++11 release
TEMPLATE = app
TARGET = lvfbterm-qt

SOURCES += main.cpp options.cpp terminalwindow.cpp consolemode.cpp evdevinput.cpp
HEADERS += options.h terminalwindow.h consolemode.h evdevinput.h

INCLUDEPATH += /usr/include/qtermwidget5
LIBS += -lqtermwidget5 /usr/lib/arm-linux-gnueabihf/libutf8proc.so.2

QMAKE_CXXFLAGS += -O2 -Wall -Wextra
