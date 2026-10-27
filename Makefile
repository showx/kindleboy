# kindleboy — Game Boy emulator for jailbroken Kindle (KUAL) + desktop dev frontend.
#
# Targets:
#   make desktop            SDL2 build for Linux/WSLg (needs libsdl2-dev)
#   make fbink              cross-build the FBInk static lib (needs koxtoolchain)
#   make kindle             cross-build the Kindle binary (depends on fbink)
#   make deploy KINDLE=/mnt/x   copy extension + binary to a mounted Kindle
#   make headless / make sim    off-device test harnesses (native cc)
#   make clean
#
# Cross toolchain: koxtoolchain "kindlepw2" (arm-kindlepw2-linux-gnueabi), on
# PATH. Static + softfp so one binary runs on 10th and 11th gen Kindles.

CROSS      ?= arm-kindlepw2-linux-gnueabi
CROSS_CC    = $(CROSS)-gcc

FBINK_DIR   = vendor/FBInk
FBINK_LIB   = build/libfbink.a

INCLUDES    = -Ivendor/walnut_cgb -Ivendor/peanut_gb -Ivendor/minigb_apu -Isrc/core -Isrc/platform
WARN        = -Wall -Wextra
STD         = -std=gnu11

# Shared, platform-independent sources.
CORE_SRC = \
	src/main.c \
	src/core/emu.c src/core/render.c src/core/ui.c src/core/status.c src/core/config.c \
	src/core/overlay.c src/core/browser.c src/core/menu.c src/core/app.c \
	src/core/peanut_impl.c \
	vendor/minigb_apu/minigb_apu.c

KINDLE_SRC = $(CORE_SRC) \
	src/platform/kindle/display_fbink.c \
	src/platform/kindle/input_evdev.c \
	src/platform/kindle/system_kindle.c \
	src/platform/kindle/audio_kindle.c

DESKTOP_SRC = $(CORE_SRC) src/platform/desktop/platform_sdl.c

.PHONY: all desktop kindle fbink deploy headless sim statetest clean cleanall

all: desktop

# ---- desktop (SDL2, native/WSLg) -----------------------------------------
desktop: build/kindleboy_desktop
build/kindleboy_desktop: $(DESKTOP_SRC)
	@mkdir -p build
	$(CC) $(STD) -O2 $(WARN) -DPLATFORM_DESKTOP $(INCLUDES) \
		$(shell sdl2-config --cflags) \
		$(DESKTOP_SRC) $(shell sdl2-config --libs) -lm -o $@
	@echo "built $@"

# ---- FBInk static lib (cross) --------------------------------------------
fbink: $(FBINK_LIB)
$(FBINK_LIB): $(FBINK_DIR)/fbink.c $(FBINK_DIR)/qimagescale/qimagescale.c
	@test -d $(FBINK_DIR) || { echo "FBInk source missing. Run:"; \
	  echo "  python scripts/fetch-deps.py"; exit 1; }
	@mkdir -p build/fbink
	$(CROSS_CC) -std=gnu11 -O2 -D_GNU_SOURCE -DFBINK_FOR_KINDLE -DFBINK_MINIMAL -DFBINK_WITH_DRAW -DFBINK_WITH_IMAGE -DFBINK_VERSION='"886f25f"' -I$(FBINK_DIR) -c $(FBINK_DIR)/fbink.c -o build/fbink/fbink.o
	$(CROSS_CC) -std=gnu11 -O2 -DFBINK_QIS_NO_SIMD -I$(FBINK_DIR) -c $(FBINK_DIR)/qimagescale/qimagescale.c -o build/fbink/qimagescale.o
	$(CROSS)-ar rcs $@ build/fbink/fbink.o build/fbink/qimagescale.o
	@echo "built $(FBINK_LIB)"

# ---- Kindle binary (cross, static) ---------------------------------------
kindle: build/kindleboy
build/kindleboy: $(KINDLE_SRC) $(FBINK_LIB)
	@mkdir -p build
	$(CROSS_CC) $(STD) -O3 -mtune=cortex-a9 -fomit-frame-pointer $(WARN) -DPLATFORM_KINDLE $(INCLUDES) -I$(FBINK_DIR) \
		$(KINDLE_SRC) $(FBINK_LIB) -lm -lrt -static -o $@
	$(CROSS)-strip $@ || true
	@echo "built $@"

# ---- deploy to a mounted Kindle ------------------------------------------
# KINDLE must point at the Kindle's USB mount (the drive that contains
# documents/ and, after this, extensions/).
deploy: build/kindleboy
	@test -n "$(KINDLE)" || { echo "usage: make deploy KINDLE=/path/to/kindle/mount"; exit 1; }
	mkdir -p kual/kindleboy/bin
	cp -f build/kindleboy kual/kindleboy/bin/kindleboy
	mkdir -p "$(KINDLE)/extensions"
	cp -rf kual/kindleboy "$(KINDLE)/extensions/"
	mkdir -p "$(KINDLE)/roms/gb"
	@echo "deployed. On the Kindle: KUAL -> kindleboy. Put *.gb in roms/gb."

# ---- test harnesses (native) ---------------------------------------------
TEST_CORE = src/core/emu.c src/core/render.c src/core/ui.c src/core/status.c src/core/config.c \
	src/core/overlay.c src/core/browser.c src/core/menu.c src/core/app.c \
	src/core/peanut_impl.c vendor/minigb_apu/minigb_apu.c

headless: build/headless
build/headless: test/headless.c src/core/emu.c src/core/render.c src/core/peanut_impl.c vendor/minigb_apu/minigb_apu.c
	@mkdir -p build
	$(CC) $(STD) -O2 $(WARN) $(INCLUDES) $^ -lm -o $@

sim: build/simrun
build/simrun: test/simrun.c $(TEST_CORE) src/platform/sim/platform_sim.c
	@mkdir -p build
	$(CC) $(STD) -O2 -w $(INCLUDES) $^ -lm -o $@

statetest: build/statetest
build/statetest: test/statetest.c src/core/emu.c src/core/peanut_impl.c vendor/minigb_apu/minigb_apu.c
	@mkdir -p build
	$(CC) $(STD) -O2 $(WARN) $(INCLUDES) $^ -lm -o $@

clean:
	rm -rf build

cleanall: clean
	-$(MAKE) -C $(FBINK_DIR) clean 2>/dev/null || true
