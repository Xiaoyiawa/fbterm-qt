# fbterm-qt — cross-compiled Qt5/QTermWidget terminal
#
# The target's Qt/qtermwidget headers and arm libraries are copied into
# build/sysroot; moc is generated on the target. Nothing is compiled natively.

TOOL := fbterm-qt

.PHONY: all sysroot cross install clean distclean

all: cross

sysroot:
	./scripts/fetch-sysroot.sh

cross:
	./scripts/cross-build.sh

install: cross
	./scripts/install-target.sh

clean:
	rm -rf build

distclean: clean
