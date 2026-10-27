#!/bin/sh
# Transfer requests can retry. Consume the local action file once under a lock.
EXT=/mnt/us/extensions/kindleboy
LOCK=/var/tmp/kindleboy-browser-action.lock
mkdir "$LOCK" 2>/dev/null || exit 0
trap 'rmdir "$LOCK" 2>/dev/null' 0
[ -f "$EXT/browser-action.request" ] || exit 0
ACTION="$(cat "$EXT/browser-action.request")"
rm -f "$EXT/browser-action.request" || exit 1
case "$ACTION" in
    play)
        sh "$EXT/bin/diagnose.sh"
        sh "$EXT/bin/run.sh"
        ;;
    setup)
        sh "$EXT/bin/install-autostart.sh" || exit 1
        # The manual test is this boot's one session; restart tests a fresh boot.
        sh "$EXT/bin/autostart.sh" --mark-current-boot || exit 1
        sh "$EXT/bin/diagnose.sh"
        sh "$EXT/bin/run.sh" stopfw
        ;;
    diagnose) sh "$EXT/bin/diagnose.sh" ;;
    *) exit 2 ;;
esac
