# lvfbterm build
#
# Cross compiler:
#   make CROSS_COMPILE=$HOME/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-
#
# Outputs:
#   bin/lvfbterm
#   build/deps/lib/liblvgl.a
#   build/deps/lib/libvterm.so*
#   build/deps/lib/libfreetype.so*

TOOL            := lvfbterm
ROOT            := $(CURDIR)
LVGL_DIR        ?= $(ROOT)/lvgl
VTERM_DIR       ?= $(ROOT)/libvterm
FREETYPE_DIR    ?= $(ROOT)/freetype

CROSS_COMPILE   ?= $(HOME)/x-tools/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-
CC              := $(CROSS_COMPILE)gcc
CXX             := $(CROSS_COMPILE)g++
AR              := $(CROSS_COMPILE)ar
RANLIB          := $(CROSS_COMPILE)ranlib
STRIP           := $(CROSS_COMPILE)strip

JOBS            ?= $(shell nproc 2>/dev/null || echo 4)
OPT             ?= -O2 -g
ARCHFLAGS       ?= -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard

BUILD           := $(ROOT)/build
DEPS            := $(BUILD)/deps
OBJ             := $(BUILD)/obj
BIN             := $(ROOT)/bin/$(TOOL)
FONT            ?= $(ROOT)/assets/unifont.otf

COMMON_DEFS     := -D_GNU_SOURCE -DLV_CONF_INCLUDE_SIMPLE
COMMON_INC      := -I$(ROOT)/config -I$(ROOT)/src -I$(LVGL_DIR) -I$(LVGL_DIR)/src \
                   -I$(VTERM_DIR)/include -I$(FREETYPE_DIR)/include

CFLAGS          := $(OPT) $(ARCHFLAGS) -pipe -fPIC -std=gnu11 -Wall -Wextra \
                   -Wno-unused-parameter -Wno-missing-field-initializers \
                   $(COMMON_DEFS) $(COMMON_INC)
CXXFLAGS        := $(OPT) $(ARCHFLAGS) -pipe -fPIC -std=gnu++11 -pthread -Wall -Wextra \
                   -Wno-unused-parameter -Wno-missing-field-initializers \
                   $(COMMON_DEFS) $(COMMON_INC)

LVGL_CFLAGS     := $(OPT) $(ARCHFLAGS) -pipe -fPIC -std=gnu11 -w $(COMMON_DEFS) $(COMMON_INC)
VTERM_CFLAGS    := $(OPT) $(ARCHFLAGS) -pipe -fPIC -std=gnu11 -w $(COMMON_DEFS) \
                   -I$(VTERM_DIR)/include -I$(VTERM_DIR)/src

LVGL_SRCS       := $(shell find $(LVGL_DIR)/src -name '*.c' | sort)
LVGL_OBJS       := $(patsubst $(LVGL_DIR)/src/%.c,$(OBJ)/lvgl/%.o,$(LVGL_SRCS))
LVGL_LIB        := $(DEPS)/lib/liblvgl.a

VTERM_SRCS      := $(shell find $(VTERM_DIR)/src -maxdepth 1 -name '*.c' | sort)
VTERM_OBJS      := $(patsubst $(VTERM_DIR)/src/%.c,$(OBJ)/vterm/%.o,$(VTERM_SRCS))
VTERM_SO        := $(DEPS)/lib/libvterm.so

APP_SRCS        := $(shell find $(ROOT)/src -name '*.cpp' | sort)
APP_OBJS        := $(patsubst $(ROOT)/src/%.cpp,$(OBJ)/app/%.o,$(APP_SRCS))

FT_BUILD        := $(BUILD)/freetype
FT_STAMP        := $(FT_BUILD)/.configured
FT_LIB          := $(DEPS)/lib/libfreetype.so

.PHONY: all deps app probe bridge clean distclean install deploy run help

all: $(BIN)

app: $(BIN)

# Build a small framebuffer diagnostic for the target.
probe: $(BUILD)/fb_probe

qt:
	@echo "Qt version is built on the target (Qt5 + QTermWidget):"
	@echo "  ssh root@192.168.1.15"
	@echo "  cd /root/lvfbterm/qt && ./build.sh"

bridge: $(BUILD)/fb_ffmpeg_bridge

$(BUILD)/fb_ffmpeg_bridge: $(ROOT)/tools/fb_ffmpeg_bridge.c
	@mkdir -p $(@D)
	$(CC) $(OPT) $(ARCHFLAGS) -std=gnu11 -Wall -Wextra -o $@ $<

$(BUILD)/fb_probe: $(ROOT)/tools/fb_probe.c
	@mkdir -p $(@D)
	$(CC) $(OPT) $(ARCHFLAGS) -std=gnu11 -Wall -Wextra -o $@ $<

deps: $(LVGL_LIB) $(VTERM_SO) $(FT_LIB)

# ---------------------------------------------------------------------------
# FreeType (shared library)
# ---------------------------------------------------------------------------
$(FREETYPE_DIR)/builds/unix/configure:
	@echo "== freetype: autogen"
	cd $(FREETYPE_DIR) && ./autogen.sh

$(FT_STAMP): $(FREETYPE_DIR)/builds/unix/configure
	@mkdir -p $(FT_BUILD) $(DEPS)
	@echo "== freetype: configure"
	cd $(FT_BUILD) && $(FREETYPE_DIR)/configure \
		--srcdir=$(FREETYPE_DIR) \
		--host=arm-unknown-linux-gnueabihf \
		--build=$(shell uname -m)-pc-linux-gnu \
		--prefix=$(DEPS) \
		--enable-shared --disable-static \
		--with-zlib=no --with-bzip2=no --with-png=no \
		--with-harfbuzz=no --with-brotli=no \
		CC="$(CC)" AR="$(AR)" RANLIB="$(RANLIB)" \
		CFLAGS="$(OPT) $(ARCHFLAGS) -fPIC"
	@echo "== freetype: make"
	$(MAKE) -C $(FT_BUILD) -j$(JOBS)
	@echo "== freetype: install"
	$(MAKE) -C $(FT_BUILD) install
	@touch $@

$(FT_LIB): $(FT_STAMP)
	@test -e $@ || (echo "freetype install did not create $@" && exit 1)

# ---------------------------------------------------------------------------
# libvterm (shared library)
# ---------------------------------------------------------------------------
$(OBJ)/vterm/%.o: $(VTERM_DIR)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(VTERM_CFLAGS) -MMD -MP -c $< -o $@

$(VTERM_SO): $(VTERM_OBJS)
	@mkdir -p $(@D)
	$(CC) $(ARCHFLAGS) -shared -Wl,-soname,libvterm.so.0 -o $@.0.0.0 $^
	ln -sf libvterm.so.0.0.0 $@.0
	ln -sf libvterm.so.0 $@
	@echo "== libvterm: $(@).0.0.0"

# ---------------------------------------------------------------------------
# LVGL (static library)
# ---------------------------------------------------------------------------
$(LVGL_OBJS): $(ROOT)/config/lv_conf.h

$(OBJ)/lvgl/%.o: $(LVGL_DIR)/src/%.c
	@mkdir -p $(@D)
	$(CC) $(LVGL_CFLAGS) -MMD -MP -c $< -o $@

$(LVGL_LIB): $(LVGL_OBJS)
	@mkdir -p $(@D)
	$(AR) rcs $@ $^
	$(RANLIB) $@
	@echo "== lvgl: $@"

# ---------------------------------------------------------------------------
# lvfbterm
# ---------------------------------------------------------------------------
$(APP_OBJS): $(ROOT)/config/lv_conf.h

$(OBJ)/app/%.o: $(ROOT)/src/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(BIN): $(APP_OBJS) $(LVGL_LIB) $(VTERM_SO) $(FT_LIB)
	@mkdir -p $(@D)
	$(CXX) $(ARCHFLAGS) -o $@ $(APP_OBJS) $(LVGL_LIB) \
		-L$(DEPS)/lib -lvterm -lfreetype -lm -lutil \
		-Wl,-rpath,'$$ORIGIN/../lib' \
		-Wl,-rpath-link,$(DEPS)/lib \
		-Wl,--as-needed -pthread -static-libstdc++ -static-libgcc
	@echo "== built: $@"
	@$(CROSS_COMPILE)readelf -d $@ | grep -E 'RPATH|RUNPATH|NEEDED' || true

clean:
	rm -rf $(OBJ) $(BIN)

distclean: clean
	rm -rf $(BUILD) $(ROOT)/bin

# ---------------------------------------------------------------------------
# Install / deploy
# ---------------------------------------------------------------------------
install: all $(FONT)
	@mkdir -p $(DESTDIR)/bin $(DESTDIR)/lib $(DESTDIR)/share
	install -m 0755 $(BIN) $(DESTDIR)/bin/
	cp -a $(DEPS)/lib/libvterm.so* $(DESTDIR)/lib/
	cp -a $(DEPS)/lib/libfreetype.so* $(DESTDIR)/lib/
	install -m 0644 $(FONT) $(DESTDIR)/share/unifont.otf
	@echo "installed into $(DESTDIR)"

$(FONT):
	@mkdir -p $(ROOT)/assets
	@echo "== extracting unifont.otf"
	tar xzf unifont-18.0.01.tar.gz -C $(ROOT)/assets --strip-components=3 \
		unifont-18.0.01/font/precompiled/unifont-18.0.01.otf
	mv $(ROOT)/assets/unifont-18.0.01.otf $(FONT)

deploy: all $(FONT)
	@DEPLOY_HOST=$(DEPLOY_HOST) DEPLOY_DIR=$(DEPLOY_DIR) ./scripts/deploy.sh

run:
	@ssh root@192.168.1.15 'cd /root/lvfbterm && ./bin/lvfbterm --font-path=share/unifont.otf'

help:
	@echo "targets: all deps app probe bridge install deploy clean distclean"
	@echo "CROSS_COMPILE=$(CROSS_COMPILE)"
	@echo "LVGL_DIR=$(LVGL_DIR)"
	@echo "VTERM_DIR=$(VTERM_DIR)"
	@echo "FREETYPE_DIR=$(FREETYPE_DIR)"

-include $(APP_OBJS:.o=.d) $(LVGL_OBJS:.o=.d) $(VTERM_OBJS:.o=.d)
