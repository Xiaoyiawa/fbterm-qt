#!/bin/sh
# Build the Qt/QTermWidget version on the target (native armhf).
# Requires: g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev
set -eu
cd "$(dirname "$0")"
qmake lvfbterm-qt.pro
make -j"${JOBS:-4}"
mkdir -p ../bin
cp -f lvfbterm-qt ../bin/lvfbterm
echo "built and installed: ../bin/lvfbterm"
