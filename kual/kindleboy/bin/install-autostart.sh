#!/bin/sh
# Install only our own Upstart job. Preserve the original root mount state and
# roll back the job if validation fails. The USB disable flag is never removed.
set -eu
EXT=/mnt/us/extensions/kindleboy
DEST=/etc/upstart/kindleboy.conf
LOG=/mnt/us/kindleboy-setup.log
BACKUP="$EXT/backups/kindleboy.conf.previous"
exec >>"$LOG" 2>&1
echo "=== autostart installation $(date) ==="
[ "$(id -u)" = 0 ] || { echo 'ERROR: root is required'; exit 1; }
grep -q '^Kindle 5\.15\.1\.1\([ (]\|$\)' /mnt/us/system/version.txt || {
    echo 'ERROR: this setup targets the confirmed KT3 / 5.15.1.1'; exit 1;
}
[ -f /etc/upstart/framework.conf ] || exit 1
[ -f "$EXT/kindleboy.upstart.conf" ] || exit 1
[ -f "$EXT/bin/autostart.sh" ] || exit 1
[ -f "$EXT/bin/kindleboy" ] || exit 1
if [ -f "$DEST" ] && ! grep -q '^# kindleboy-managed-v1$' "$DEST"; then
    echo 'ERROR: unrecognized existing kindleboy.conf; left untouched'
    exit 1
fi

WAS_RO=0
if awk '$2=="/" && $4 ~ /(^|,)ro(,|$)/ {found=1} END {exit !found}' /proc/mounts; then
    WAS_RO=1
fi
CHANGED=0
HAD_JOB=0
SUCCESS=0
mkdir -p "$EXT/backups"
if [ -f "$DEST" ]; then
    cp "$DEST" "$BACKUP"
    HAD_JOB=1
fi
finish() {
    result=$?
    trap - 0
    if [ "$SUCCESS" != 1 ] && [ "$CHANGED" = 1 ]; then
        if [ "$HAD_JOB" = 1 ]; then cp "$BACKUP" "$DEST"; else rm -f "$DEST"; fi
        initctl reload-configuration || true
    fi
    rm -f "$DEST.kindleboy-tmp"
    sync
    if [ "$WAS_RO" = 1 ]; then mntroot ro || result=1; fi
    exit "$result"
}
trap finish 0
if [ "$WAS_RO" = 1 ]; then mntroot rw; fi
cp "$EXT/kindleboy.upstart.conf" "$DEST.kindleboy-tmp"
chmod 0644 "$DEST.kindleboy-tmp"
mv -f "$DEST.kindleboy-tmp" "$DEST"
CHANGED=1
initctl reload-configuration
initctl status kindleboy
printf 'enabled\n' >"$EXT/autostart.enabled"
SUCCESS=1
echo 'status=installed'
echo 'USB recovery: create DISABLE_KINDLEBOY in the USB drive root to skip autostart.'
