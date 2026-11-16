#!/bin/sh
# Fetch the armhf Qt/QTermWidget development files and generate moc output
# from the target. Run from the project root.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TARGET=${TARGET:-root@192.168.1.15}
SYSROOT=${SYSROOT:-$ROOT/build/sysroot}
MOCDIR=${MOCDIR:-$ROOT/build/moc}
SSH_CMD=${SSH_CMD:-ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null}

mkdir -p "$SYSROOT/usr" "$MOCDIR"

echo "== headers/libs from $TARGET"
$SSH_CMD "$TARGET" 'cd /usr && tar czf - \
    include/arm-linux-gnueabihf/qt5 \
    include/qtermwidget5 \
    lib/arm-linux-gnueabihf/libQt5*.so* \
    lib/arm-linux-gnueabihf/libqtermwidget5.so* \
    lib/arm-linux-gnueabihf/libutf8proc.so* \
    lib/arm-linux-gnueabihf/libGLESv2.so*' | tar xzf - -C "$SYSROOT/usr"

echo "== moc from $TARGET"
tar czf - qt | $SSH_CMD "$TARGET" 'rm -rf /tmp/fbterm-moc-src && mkdir -p /tmp/fbterm-moc-src && tar xzf - -C /tmp/fbterm-moc-src'
$SSH_CMD "$TARGET" "cd '/tmp/fbterm-moc-src/qt' && \
    /usr/lib/qt5/bin/moc -I. -I/usr/include/qtermwidget5 \
        -I/usr/include/arm-linux-gnueabihf/qt5 \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtCore \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtGui \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtWidgets \
        terminalwindow.h -o moc_terminalwindow.cpp && \
    /usr/lib/qt5/bin/moc -I. -I/usr/include/qtermwidget5 \
        -I/usr/include/arm-linux-gnueabihf/qt5 \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtCore \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtGui \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtWidgets \
        evdevinput.h -o moc_evdevinput.cpp && \
    /usr/lib/qt5/bin/moc -I. -I/usr/include/qtermwidget5 \
        -I/usr/include/arm-linux-gnueabihf/qt5 \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtCore \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtGui \
        -I/usr/include/arm-linux-gnueabihf/qt5/QtWidgets \
        signalbridge.h -o moc_signalbridge.cpp && \
    tar czf - moc_terminalwindow.cpp moc_evdevinput.cpp moc_signalbridge.cpp" | tar xzf - -C "$MOCDIR"

echo "sysroot: $SYSROOT"
echo "moc:     $MOCDIR"
