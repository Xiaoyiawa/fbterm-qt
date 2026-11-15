# lvfbterm — Qt5 / QTermWidget version
#
# The Qt application is built natively on the target because Qt/qtermwidget
# dev packages are installed there. The host cross compiler is only used for
# the small evdev/framebuffer test tools.

TOOL            := lvfbterm
CROSS_COMPILE   ?= $(HOME)/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-
CC              := $(CROSS_COMPILE)gcc
ARCHFLAGS       ?= -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard
OPT             ?= -O2 -g
BUILD           := build

.PHONY: all qt qt-clean probe uinput deploy clean distclean

all: probe uinput
	@echo "Qt app: build on the target with 'cd qt && ./build.sh'"
	@echo "or from here: make qt (if qmake is available)"

qt:
	@if command -v qmake >/dev/null 2>&1; then \
		cd qt && ./build.sh; \
	else \
		echo "qmake not found on this host."; \
		echo "On the target: cd /root/lvfbterm/qt && ./build.sh"; \
	fi

qt-clean:
	cd qt && rm -f *.o moc_* Makefile .qmake.stash lvfbterm-qt

$(BUILD)/fb_probe: tools/fb_probe.c
	@mkdir -p $(@D)
	$(CC) $(OPT) $(ARCHFLAGS) -std=gnu11 -Wall -Wextra -o $@ $<

$(BUILD)/uinput_type: tools/uinput_type.c
	@mkdir -p $(@D)
	$(CC) $(OPT) $(ARCHFLAGS) -std=gnu11 -Wall -Wextra -o $@ $<

probe: $(BUILD)/fb_probe
uinput: $(BUILD)/uinput_type

deploy:
	./scripts/deploy.sh

clean: qt-clean
	rm -rf $(BUILD)

distclean: clean
