#!/bin/sh
#
# Install and enable the lvfbterm systemd service on the target.
# Run as root from the deployed tree, e.g. /root/lvfbterm.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)

install -m 0644 "$ROOT/systemd/lvfbterm.service" /etc/systemd/system/lvfbterm.service
systemctl daemon-reload

# Free VT1 for the framebuffer terminal; it conflicts with getty@tty1.
systemctl disable --now getty@tty1.service 2>/dev/null || true

systemctl enable lvfbterm.service
systemctl restart lvfbterm.service
sleep 1
systemctl --no-pager --full status lvfbterm.service || true
