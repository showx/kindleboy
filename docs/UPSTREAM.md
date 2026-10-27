# Source and modifications

This project incorporates **KindleBoy** by itsParassharma, distributed under
GPL-3.0-or-later. It is a local adaptation, not an official upstream release.

- Repository: https://github.com/itsParassharma/KindleBoy
- Imported commit: `b79baf19929586d4609ff38db0020589a8232aa1`
- Original README: [UPSTREAM_README.md](UPSTREAM_README.md)
- Original licenses and third-party notices are preserved in `LICENSE`,
  `NOTICE.md`, and each vendored library's license/header.
- FBInk and its stb submodule are pinned in `dependencies.lock.json` and fetched
  for builds. The source package includes them and their notices.

Local changes include KT3 build/packaging scripts, modern scriptlet launchers,
launcher cleanup on termination, concurrent-launch prevention, automatic state
save when leaving a game, corrected grayscale test output, an original DMG demo,
and device-specific Chinese installation instructions.

The local UI uses a shared Game Boy shell for the cartridge picker, gameplay
and pause menu, with a grayscale bezel, cross-shaped D-pad, circular A/B,
SELECT/START pills and speaker slots. Touch targets retain diagonal and combined
input; menus also accept virtual controls. Selection refreshes stay within the
black-and-white LCD so they do not degrade the shell's static gray shades.
Press feedback adds key-cap travel, collapsing shadows, inverted A/B faces and
direction highlights, including combined input. Only changed control bounds
refresh, with B/W shadow textures and coalesced short-tap feedback; no idle
animation. Input refreshes are independent of the game refresh busy clock.
On KT3 the game remains 3x integer scaled. UI geometry/input checks run at three
panel sizes with sanitizer instrumentation; feedback tests also check local
damage, queued taps, release restoration and continued game refreshes.
On-device verification of this UI
revision is pending, distinct from the functional checks below.

Dedicated boot support uses a project-owned Upstart job with one run per kernel
boot, a USB disable flag, USB storage checks, and rollback on failed job
validation. System setup and browser actions require on-device execution; file
copying alone does not install a root filesystem job.

FBInk is compiled directly with its minimal image/drawing feature flags. This
avoids the upstream Makefile's unconditional prerequisite on the Kobo-only I2C
library. No I2C functionality is used by the Kindle target.

The active emulation core in this revision is Walnut-CGB, descended from
Peanut-GB. The upstream README still mentions Peanut-GB as the active core.

Normal mode has been checked on KT3 / firmware 5.15.1.1: grayscale rendering,
A/B input, state save/load, battery save output, and clean exit. The user also
confirmed that Setup reports SUCCESS and restarting the device automatically
opens Game Boy. Multi-touch combinations, performance across games,
suspend/resume, and long sessions have not had dedicated validation.
