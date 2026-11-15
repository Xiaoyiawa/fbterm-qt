#!/bin/sh
#
# Build the Qt version on the target over SSH (source is copied, compiled
# there, then installed as /root/lvfbterm/bin/lvfbterm).
#
# Usage:
#   DEPLOY_HOST=root@192.168.1.15 DEPLOY_DIR=/root/lvfbterm ./scripts/deploy.sh
#
# The target needs: g++ qtbase5-dev qtbase5-dev-tools libqtermwidget5-0-dev

set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOST=${DEPLOY_HOST:-root@192.168.1.15}
DIR=${DEPLOY_DIR:-/root/lvfbterm}

cd "$ROOT"

tar czf - qt share | ssh "$HOST" "
    set -e
    mkdir -p '$DIR'
    tar xzf - -C '$DIR'
    cd '$DIR/qt'
    ./build.sh
    cd '$DIR'
    ./bin/lvfbterm --help
"

echo "== deployed to $HOST:$DIR"
