# Licensing

**kindleboy** as a whole is distributed under the **GNU General Public License v3.0
or later** (see `LICENSE`). This is required because the Kindle build links
against FBInk, which is GPL-3.0-or-later.

## Third-party components

| Component | Where | License |
|-----------|-------|---------|
| Walnut-CGB (`vendor/walnut_cgb/walnut_cgb.h`) | vendored, the active core | MIT — Copyright (c) 2025 Mr-PauI |
| Peanut-GB (`vendor/peanut_gb/peanut_gb.h`) | vendored, kept as a DMG-only fallback | MIT — Copyright (c) 2018-2023 Mahyar Koshkouei |
| minigb_apu (`vendor/minigb_apu/`) | vendored, optional sound | MIT — Copyright (c) 2017 Alex Baines, 2019 Mahyar Koshkouei |
| font8x8 (`vendor/font8x8_basic.h`) | vendored | Public Domain — Daniel Hepper / Marcel Sondaar (IBM VGA fonts) |
| FBInk (`vendor/FBInk/`) | cloned at build time, **not committed** | GPL-3.0-or-later — NiLuJe |
| stb (FBInk image support) | fetched at the pinned FBInk submodule revision | MIT or public domain; license included in package |
| QImageScale (FBInk image support) | `vendor/FBInk/qimagescale/` | Qt Company; distributed here under its GPL-3.0 option |

The MIT and public-domain components are GPL-compatible. Only the Kindle target
links FBInk; the desktop dev frontend (SDL2) does not, but the combined source
tree is offered under GPL-3.0-or-later for simplicity.

## ROMs

No game ROMs are included. Game Boy games are copyrighted; supply your own
legally-obtained dumps. `dmg-acid2`, `cgb-acid2` and Blargg's test ROMs (used
only for development) are freely distributed test software.
