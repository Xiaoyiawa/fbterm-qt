# fbterm-qt — cross-compilable Qt5/QTermWidget terminal
#
# Uses the standard environment toolchain variables:
#   CC CXX LD AR RANLIB
# and standard flags:
#   CPPFLAGS CXXFLAGS LDFLAGS LDLIBS
#
# For the arm target:
#   export CC=arm-unknown-linux-gnueabihf-gcc
#   export CXX=arm-unknown-linux-gnueabihf-g++
#   export LD=arm-unknown-linux-gnueabihf-g++
#   export AR=arm-unknown-linux-gnueabihf-ar
#   export RANLIB=arm-unknown-linux-gnueabihf-ranlib
#   make SYSROOT=/path/to/sysroot
#   make install
#
# build/fbterm-qt        compiled binary
# build/usr/             staged install tree (for copying to / on target)

CC      ?= cc
CXX     ?= c++
AR      ?= ar
RANLIB  ?= ranlib

# Make's built-in LD is "ld"; default to the C++ driver so that libstdc++
# and the target linker script are handled correctly. An explicit LD wins.
ifeq ($(origin LD),default)
LD := $(CXX)
endif

BUILD    ?= build
PREFIX   ?= /usr
DESTDIR  ?= $(BUILD)
STAGE    := $(DESTDIR)$(PREFIX)
SYSROOT  ?= $(BUILD)/sysroot
QTINC    ?= $(SYSROOT)/usr/include/arm-linux-gnueabihf/qt5
QTERM    ?= $(SYSROOT)/usr/include/qtermwidget5
LIBDIR   ?= $(SYSROOT)/usr/lib/arm-linux-gnueabihf

TARGET      := $(BUILD)/fbterm-qt
OBJDIR      := $(BUILD)/obj
LIBDIR_OUT  := $(BUILD)/lib
PINYIN_LIB  := $(LIBDIR_OUT)/libpinyinime.a

APP_SOURCES := \
    qt/main.cpp \
    qt/options.cpp \
    qt/terminalwindow.cpp \
    qt/consolemode.cpp \
    qt/evdevinput.cpp \
    qt/signalbridge.cpp \
    qt/ime.cpp

PINYIN_SOURCES := \
    qt/vendor/pinyinime/src/pinyin_ime.cpp \
    qt/vendor/pinyinime/src/utf8.cpp

MOC_SOURCES := \
    qt/generated/moc_terminalwindow.cpp \
    qt/generated/moc_evdevinput.cpp \
    qt/generated/moc_signalbridge.cpp

APP_OBJECTS    := $(patsubst %.cpp,$(OBJDIR)/%.o,$(APP_SOURCES) $(MOC_SOURCES))
PINYIN_OBJECTS := $(patsubst %.cpp,$(OBJDIR)/%.o,$(PINYIN_SOURCES))

CPPFLAGS += \
    -Iqt \
    -Iqt/vendor/pinyinime/include \
    -Iqt/vendor/pinyinime/src \
    -I$(QTINC) -I$(QTINC)/QtCore -I$(QTINC)/QtGui -I$(QTINC)/QtWidgets \
    -I$(QTERM)

CXXFLAGS ?= -O2 -std=c++17 -fPIC -DQT_NO_DEBUG -Wall -Wextra

LDFLAGS += \
    -L$(LIBDIR) \
    -Wl,-rpath-link,$(LIBDIR) \
    -Wl,-rpath,/usr/lib/arm-linux-gnueabihf \
    -Wl,--allow-shlib-undefined

LDLIBS += \
    -l:libqtermwidget5.so.0 \
    -l:libQt5Widgets.so.5 \
    -l:libQt5Gui.so.5 \
    -l:libQt5Core.so.5 \
    -l:libutf8proc.so.2 \
    -l:libGLESv2.so.2 \
    -lpthread

.PHONY: all install clean distclean moc

all: $(TARGET)

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(PINYIN_LIB): $(PINYIN_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^
	$(RANLIB) $@

$(TARGET): $(APP_OBJECTS) $(PINYIN_LIB)
	@mkdir -p $(dir $@)
	$(LD) $(LDFLAGS) -o $@ $(APP_OBJECTS) $(PINYIN_LIB) $(LDLIBS)

install: $(TARGET)
	rm -rf $(STAGE)
	install -Dm755 $(TARGET) $(STAGE)/bin/fbterm-qt
	install -d $(STAGE)/share/fbterm-qt/color-schemes
	install -m644 share/pinyin.txt $(STAGE)/share/fbterm-qt/pinyin.txt
	install -m644 share/dictionary.data $(STAGE)/share/fbterm-qt/dictionary.data
	install -m644 share/color-schemes/Campbell.colorscheme $(STAGE)/share/fbterm-qt/color-schemes/Campbell.colorscheme
	install -m644 share/Unifont.otf $(STAGE)/share/fbterm-qt/Unifont.otf
	install -m644 share/Unifont.otf $(STAGE)/share/fbterm-qt/font.otf
	install -Dm644 share/keytab/fbterm-qt.keytab $(STAGE)/share/qtermwidget5/kb-layouts/fbterm-qt.keytab
	install -Dm644 systemd/fbterm-qt.service $(STAGE)/lib/systemd/system/fbterm-qt.service
	install -m644 README.md $(STAGE)/share/fbterm-qt/README.md

# Optional: regenerate moc when Q_OBJECT headers change. A host moc is needed.
moc:
	$(MOC) -Iqt -I$(QTERM) -I$(QTINC) -I$(QTINC)/QtCore -I$(QTINC)/QtGui -I$(QTINC)/QtWidgets qt/terminalwindow.h -o qt/generated/moc_terminalwindow.cpp
	$(MOC) -Iqt -I$(QTERM) -I$(QTINC) -I$(QTINC)/QtCore -I$(QTINC)/QtGui -I$(QTINC)/QtWidgets qt/evdevinput.h -o qt/generated/moc_evdevinput.cpp
	$(MOC) -Iqt -I$(QTERM) -I$(QTINC) -I$(QTINC)/QtCore -I$(QTINC)/QtGui -I$(QTINC)/QtWidgets qt/signalbridge.h -o qt/generated/moc_signalbridge.cpp

clean:
	rm -rf $(OBJDIR) $(LIBDIR_OUT) $(TARGET) $(STAGE)

distclean: clean
	rm -rf $(BUILD)

-include $(APP_OBJECTS:.o=.d) $(PINYIN_OBJECTS:.o=.d)
