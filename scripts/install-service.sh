#!/bin/sh
#
# Install and enable the fbterm-qt systemd service on the target.
# Run as root from the deployed tree, e.g. /root/fbterm-qt.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

install -m 0644 "$ROOT/systemd/fbterm-qt.service" /etc/systemd/system/fbterm-qt.service
systemctl daemon-reload

# Free VT1 for the framebuffer terminal; it conflicts with getty@tty1.
systemctl disable --now getty@tty1.service 2>/dev/null || true

systemctl enable fbterm-qt.service
systemctl restart fbterm-qt.service
sleep 1
systemctl --no-pager --full status fbterm-qt.service || true
