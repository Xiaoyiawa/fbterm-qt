#!/bin/sh
# Build the Qt/QTermWidget version on the target (native armhf).
# Requires: g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev
set -eu
cd "$(dirname "$0")"
qmake qt_fbterm.pro
make -j"${JOBS:-4}"
mkdir -p ../bin
cp -f qt_fbterm ../bin/qt_fbterm
if [ "$(id -u)" = "0" ] && [ -d /usr/share/qtermwidget5/kb-layouts ]; then
    install -m 0644 ../share/keytab/qt_fbterm.keytab \
        /usr/share/qtermwidget5/kb-layouts/qt_fbterm.keytab
    echo "installed keytab: /usr/share/qtermwidget5/kb-layouts/qt_fbterm.keytab"
fi
echo "built and installed: ../bin/qt_fbterm"
