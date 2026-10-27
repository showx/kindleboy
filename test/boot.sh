#!/bin/sh
# Run only in the disposable build container, with a fake /mnt/us and Upstart.
set -eu
EXT=/mnt/us/extensions/kindleboy
mkdir -p "$EXT/bin" /mnt/us/system /mnt/us/documents /etc/upstart /tmp/kindleboy-boot-mocks
cp -r /work/kual/kindleboy/. "$EXT/"
printf 'Kindle 5.15.1.1 (392505 005)\n' >/mnt/us/system/version.txt
touch /etc/upstart/framework.conf "$EXT/bin/kindleboy"
MOCK=/tmp/kindleboy-boot-mocks
export BOOT_CALLS=/tmp/kindleboy-boot-calls
export VALIDATE_FAIL=0 MOCK_ROOT_RO=1 DRIVE_MODE=0
cat >"$MOCK/initctl" <<'EOF'
#!/bin/sh
echo "initctl $*" >>"$BOOT_CALLS"
if [ "$1" = status ] && [ "${VALIDATE_FAIL:-0}" = 1 ]; then exit 1; fi
echo 'kindleboy stop/waiting'
EOF
cat >"$MOCK/mntroot" <<'EOF'
#!/bin/sh
echo "mntroot $*" >>"$BOOT_CALLS"
EOF
cat >"$MOCK/awk" <<'EOF'
#!/bin/sh
[ "${MOCK_ROOT_RO:-0}" = 1 ]
EOF
cat >"$MOCK/lipc-get-prop" <<'EOF'
#!/bin/sh
printf '%s\n' "${DRIVE_MODE:-}"
EOF
cat >"$MOCK/sleep" <<'EOF'
#!/bin/sh
exit 0
EOF
cat >"$EXT/bin/run.sh" <<'EOF'
#!/bin/sh
echo "run $*" >>"$BOOT_CALLS"
EOF
chmod +x "$MOCK"/*
export PATH="$MOCK:$PATH"
ONCE="/var/tmp/kindleboy-autostart.$(cat /proc/sys/kernel/random/boot_id)"

: >"$BOOT_CALLS"
sh "$EXT/bin/install-autostart.sh"
grep -q '^# kindleboy-managed-v1$' /etc/upstart/kindleboy.conf
[ -f "$EXT/autostart.enabled" ]
grep -q 'mntroot rw' "$BOOT_CALLS"
tail -n 1 "$BOOT_CALLS" | grep -q 'mntroot ro'
echo 'PASS: Upstart install validates the job and restores the root mount state'

: >"$BOOT_CALLS"
touch /mnt/us/DISABLE_KINDLEBOY
sh "$EXT/bin/autostart.sh"
[ ! -s "$BOOT_CALLS" ]
rm /mnt/us/DISABLE_KINDLEBOY
sh "$EXT/bin/autostart.sh"
sh "$EXT/bin/autostart.sh"
[ "$(grep -c '^run stopfw$' "$BOOT_CALLS")" = 1 ]
echo 'PASS: USB disable flag is honored and game starts only once per boot'

rmdir "$ONCE"
: >"$BOOT_CALLS"
DRIVE_MODE=1 sh "$EXT/bin/autostart.sh"
[ ! -s "$BOOT_CALLS" ]
rmdir "$ONCE"
DRIVE_MODE='' sh "$EXT/bin/autostart.sh"
[ ! -s "$BOOT_CALLS" ]
rmdir "$ONCE"
echo 'PASS: USB storage mode and unknown state leave the Kindle UI available'

cp /etc/upstart/kindleboy.conf /tmp/kindleboy-job-before
: >"$BOOT_CALLS"
rc=0
VALIDATE_FAIL=1 sh "$EXT/bin/install-autostart.sh" || rc=$?
[ "$rc" -ne 0 ]
cmp /etc/upstart/kindleboy.conf /tmp/kindleboy-job-before
tail -n 1 "$BOOT_CALLS" | grep -q 'mntroot ro'
echo 'PASS: failed validation restores the previous job and read-only root'

# Browser transfer retries must not repeat setup or relaunch a game.
cat >"$EXT/bin/diagnose.sh" <<'EOF'
#!/bin/sh
echo diagnostic >>"$BOOT_CALLS"
EOF
: >"$BOOT_CALLS"
printf 'play\n' >"$EXT/browser-action.request"
sh "$EXT/bin/browser-action.sh"
sh "$EXT/bin/browser-action.sh"
[ "$(grep -c '^diagnostic$' "$BOOT_CALLS")" = 1 ]
[ "$(grep -c '^run $' "$BOOT_CALLS")" = 1 ]
[ ! -f "$EXT/browser-action.request" ]
echo 'PASS: a browser action runs once, including when the transfer is retried'

sh /work/launchers/KindleBoy-Setup.sh
grep -q '^SUCCESS$' /mnt/us/documents/KindleBoy-Setup-Result.txt
[ -d "$ONCE" ]
rc=0
VALIDATE_FAIL=1 sh /work/launchers/KindleBoy-Setup.sh || rc=$?
[ "$rc" -ne 0 ]
grep -q '^FAILED$' /mnt/us/documents/KindleBoy-Setup-Result.txt
echo 'PASS: the library setup entry reports actual success or failure explicitly'
