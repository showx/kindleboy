/*
 * peanut_impl.c — the single file that actually compiles the emulator core.
 *
 * The core (Walnut-CGB, a CGB-capable rewrite of Peanut-GB) is header-only.
 * Everyone else includes emu.h, which pulls in the header in "declarations
 * only" mode. Here — and only here — we let the real function bodies through,
 * so we don't end up with duplicate symbols at link time. The knobs below
 * match the core's defaults; we set them anyway so it's obvious what we build
 * and so an upstream default change can't surprise us.
 *
 * (The filename stays peanut_impl.c out of habit / to avoid churn; Walnut-CGB
 * is a direct descendant of Peanut-GB and keeps the same API.)
 */
#define ENABLE_SOUND 1                  /* optional sound; gated at runtime (see emu.c) */
#define ENABLE_LCD   1
#define WALNUT_GB_HIGH_LCD_ACCURACY 1   /* the CPU can easily afford it */

/* With ENABLE_SOUND the core calls these two externals on APU register access;
 * they're defined in emu.c and forward to the minigb_apu instance. Declared here
 * so the core implementation compiles. */
#include <stdint.h>
uint8_t audio_read(uint16_t addr);
void    audio_write(uint16_t addr, uint8_t val);

#include "walnut_cgb.h"
