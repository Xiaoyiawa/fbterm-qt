#!/bin/sh
# Build the Qt/QTermWidget version on the target (native armhf).
# Requires: g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev
set -eu
cd "$(dirname "$0")"
qmake fbterm-qt.pro
make -j"${JOBS:-4}"
mkdir -p ../bin
cp -f fbterm-qt ../bin/fbterm-qt
if [ "$(id -u)" = "0" ] && [ -d /usr/share/qtermwidget5/kb-layouts ]; then
    install -m 0644 ../share/keytab/fbterm-qt.keytab \
        /usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab
    echo "installed keytab: /usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab"
fi
echo "built and installed: ../bin/fbterm-qt"
