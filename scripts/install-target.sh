#!/bin/sh
# Install the cross-built binary and runtime data under /usr on the target.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
TARGET=${TARGET:-root@192.168.1.15}
BIN=${BIN:-$ROOT/build/cross/fbterm-qt}
SSH_CMD=${SSH_CMD:-ssh -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null}
SCP_CMD=${SCP_CMD:-scp -F /dev/null -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null}

[ -f "$BIN" ] || { echo "missing $BIN; run scripts/cross-build.sh" >&2; exit 1; }

$SCP_CMD "$BIN" "$TARGET:/tmp/fbterm-qt"
tar czf - share systemd README.md | $SSH_CMD "$TARGET" 'rm -rf /tmp/fbterm-install && mkdir -p /tmp/fbterm-install && tar xzf - -C /tmp/fbterm-install'

$SSH_CMD "$TARGET" '
set -eu
systemctl stop fbterm-qt.service 2>/dev/null || true
systemctl disable fbterm-qt.service 2>/dev/null || true
rm -f /etc/systemd/system/fbterm-qt.service

install -Dm755 /tmp/fbterm-qt /usr/bin/fbterm-qt
install -d /usr/share/fbterm-qt/color-schemes
install -m644 /tmp/fbterm-install/share/pinyin.txt /usr/share/fbterm-qt/pinyin.txt
install -m644 /tmp/fbterm-install/share/dictionary.data /usr/share/fbterm-qt/dictionary.data
install -m644 /tmp/fbterm-install/share/color-schemes/Campbell.colorscheme /usr/share/fbterm-qt/color-schemes/
install -m644 /tmp/fbterm-install/share/keytab/fbterm-qt.keytab /usr/share/qtermwidget5/kb-layouts/fbterm-qt.keytab
install -m644 /tmp/fbterm-install/README.md /usr/share/fbterm-qt/README.md

install -Dm644 /tmp/fbterm-install/systemd/fbterm-qt.service /usr/lib/systemd/system/fbterm-qt.service
systemctl daemon-reload
systemctl enable fbterm-qt.service
systemctl restart fbterm-qt.service
sleep 2
systemctl is-enabled fbterm-qt.service
systemctl is-active fbterm-qt.service
/usr/bin/fbterm-qt --help | head -2
ls -lh /usr/bin/fbterm-qt /usr/share/fbterm-qt
'
echo "installed to $TARGET:/usr"
