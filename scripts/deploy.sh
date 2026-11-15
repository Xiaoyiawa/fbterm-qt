#!/bin/sh
#
# Build, stage, and copy lvfbterm to the target device.
#
# Usage:
#   ./scripts/deploy.sh
#   DEPLOY_HOST=root@192.168.1.15 DEPLOY_DIR=/root/lvfbterm ./scripts/deploy.sh
#
# The target needs a POSIX shell and tar (no rsync required). Authentication
# is whatever ssh uses: key, agent, or an interactive password prompt.

set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
HOST=${DEPLOY_HOST:-root@192.168.1.15}
DIR=${DEPLOY_DIR:-/root/lvfbterm}
STAGE="$ROOT/build/stage"

cd "$ROOT"

echo "== building"
make -j"${JOBS:-4}" all

echo "== staging"
rm -rf "$STAGE"
make install DESTDIR="$STAGE"

if [ ! -x "$STAGE/bin/lvfbterm" ]; then
    echo "deploy: $STAGE/bin/lvfbterm missing" >&2
    exit 1
fi

echo "== deploying to $HOST:$DIR"
tar czf - -C "$STAGE" . | ssh "$HOST" "
    set -e
    mkdir -p '$DIR'
    tar xzf - -C '$DIR'
    cd '$DIR'
    LD_LIBRARY_PATH='$DIR/lib' ./bin/lvfbterm --version
"

echo "== deployed"
echo "run:"
echo "  ssh $HOST"
echo "  cd $DIR && ./bin/lvfbterm --font-path=share/unifont.otf"
