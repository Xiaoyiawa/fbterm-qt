QT += widgets
CONFIG += c++17 release
TEMPLATE = app
TARGET = fbterm-qt

SOURCES += main.cpp options.cpp terminalwindow.cpp consolemode.cpp evdevinput.cpp signalbridge.cpp ime.cpp \
    vendor/pinyinime/src/pinyin_ime.cpp vendor/pinyinime/src/utf8.cpp
HEADERS += options.h terminalwindow.h consolemode.h evdevinput.h signalbridge.h ime.h

INCLUDEPATH += /usr/include/qtermwidget5 vendor/pinyinime/include vendor/pinyinime/src
LIBS += -lqtermwidget5 /usr/lib/arm-linux-gnueabihf/libutf8proc.so.2

QMAKE_CXXFLAGS += -O2 -Wall -Wextra
