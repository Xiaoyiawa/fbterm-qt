#!/bin/sh
#
# Install and enable the qt_fbterm systemd service on the target.
# Run as root from the deployed tree, e.g. /root/qt_fbterm.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

install -m 0644 "$ROOT/systemd/qt_fbterm.service" /etc/systemd/system/qt_fbterm.service
systemctl daemon-reload

# Free VT1 for the framebuffer terminal; it conflicts with getty@tty1.
systemctl disable --now getty@tty1.service 2>/dev/null || true

systemctl enable qt_fbterm.service
systemctl restart qt_fbterm.service
sleep 1
systemctl --no-pager --full status qt_fbterm.service || true
