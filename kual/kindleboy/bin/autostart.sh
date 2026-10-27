#!/bin/sh
EXT=/mnt/us/extensions/kindleboy
LOG=/mnt/us/kindleboy-boot.log
BOOT_ID="$(cat /proc/sys/kernel/random/boot_id 2>/dev/null)"
case "$BOOT_ID" in ''|*[!a-f0-9-]*) exit 1 ;; esac
ONCE="/var/tmp/kindleboy-autostart.$BOOT_ID"

case "${1:-}" in
    --mark-current-boot) mkdir "$ONCE" 2>/dev/null || true; exit 0 ;;
    '') ;;
    *) exit 2 ;;
esac

[ -f "$EXT/autostart.enabled" ] || exit 0
[ ! -e /mnt/us/DISABLE_KINDLEBOY ] || exit 0
# No respawn: returning to the reading UI must not launch the game again.
mkdir "$ONCE" 2>/dev/null || exit 0
echo "=== boot request $(date) ===" >>"$LOG"
sleep 5
[ -f "$EXT/autostart.enabled" ] || exit 0
[ ! -e /mnt/us/DISABLE_KINDLEBOY ] || exit 0

# Leave USB storage mode available for recovery. Unknown state also skips boot.
DRIVE_MODE="$(lipc-get-prop com.lab126.volumd driveModeState 2>/dev/null)"
if [ "$DRIVE_MODE" != 0 ]; then
    echo "Skipping: USB storage state is ${DRIVE_MODE:-unknown}" >>"$LOG"
    exit 0
fi
[ -f "$EXT/bin/kindleboy" ] || exit 1
export KB_DETACHED=1
exec /bin/sh "$EXT/bin/run.sh" stopfw >>"$LOG" 2>&1
