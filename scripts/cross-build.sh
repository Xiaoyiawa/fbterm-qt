#!/bin/sh
# Cross-build fbterm-qt for armhf using the sysroot/moc fetched from target.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
CROSS_COMPILE=${CROSS_COMPILE:-$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-}
CXX=${CROSS_COMPILE}g++
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
MOCDIR=${MOCDIR:-$ROOT/build/moc}
OUTDIR=${OUTDIR:-$ROOT/build/cross}
QTINC="$SYSROOT/usr/include/arm-linux-gnueabihf/qt5"
LIBDIR="$SYSROOT/usr/lib/arm-linux-gnueabihf"

[ -x "$CXX" ] || { echo "missing compiler: $CXX" >&2; exit 1; }
[ -f "$MOCDIR/moc_terminalwindow.cpp" ] || { echo "missing moc; run scripts/fetch-sysroot.sh" >&2; exit 1; }
[ -f "$QTINC/QtCore/qglobal.h" ] || { echo "missing Qt headers; run scripts/fetch-sysroot.sh" >&2; exit 1; }

mkdir -p "$OUTDIR"
SOURCES="$ROOT/qt/main.cpp $ROOT/qt/options.cpp $ROOT/qt/terminalwindow.cpp \
$ROOT/qt/consolemode.cpp $ROOT/qt/evdevinput.cpp $ROOT/qt/signalbridge.cpp \
$ROOT/qt/ime.cpp $ROOT/qt/vendor/pinyinime/src/pinyin_ime.cpp \
$ROOT/qt/vendor/pinyinime/src/utf8.cpp \
$MOCDIR/moc_terminalwindow.cpp $MOCDIR/moc_evdevinput.cpp $MOCDIR/moc_signalbridge.cpp"

"$CXX" -std=c++17 -O2 -fPIC -DQT_NO_DEBUG -Wall -Wextra \
    -I"$ROOT/qt" \
    -I"$ROOT/qt/vendor/pinyinime/include" \
    -I"$ROOT/qt/vendor/pinyinime/src" \
    -I"$QTINC" -I"$QTINC/QtCore" -I"$QTINC/QtGui" -I"$QTINC/QtWidgets" \
    -I"$SYSROOT/usr/include/qtermwidget5" \
    $SOURCES \
    -L"$LIBDIR" -Wl,-rpath-link,"$LIBDIR" -Wl,-rpath,/usr/lib/arm-linux-gnueabihf \
    -Wl,--allow-shlib-undefined \
    -l:libqtermwidget5.so.0 -l:libQt5Widgets.so.5 -l:libQt5Gui.so.5 -l:libQt5Core.so.5 \
    -l:libutf8proc.so.2 -l:libGLESv2.so.2 -lpthread \
    -o "$OUTDIR/fbterm-qt"

file "$OUTDIR/fbterm-qt"
echo "built: $OUTDIR/fbterm-qt"
