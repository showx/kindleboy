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
