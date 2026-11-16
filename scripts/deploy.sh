#!/bin/sh
# Cross-build on the host, then install under /usr on the target.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
[ -f build/moc/moc_terminalwindow.cpp ] || ./scripts/fetch-sysroot.sh
./scripts/cross-build.sh
./scripts/install-target.sh
