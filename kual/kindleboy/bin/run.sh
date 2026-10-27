#!/bin/sh
# GPL-3.0-or-later. Based on itsParassharma/KindleBoy; see docs/UPSTREAM.md.
EXT="$(cd "$(dirname "$0")/.." 2>/dev/null && pwd)" || exit 1
LOG=/mnt/us/kindleboy.log
SESSION=/var/tmp/kindleboy-session

case "${1:-}" in
    ''|stopfw) ;;
    *) echo 'usage: sh run.sh [stopfw]' >&2; exit 2 ;;
esac

if [ "${KB_DETACHED:-0}" != 1 ]; then
    command -v setsid >/dev/null 2>&1 || exit 1
    KB_DETACHED=1 setsid sh "$0" "$@" </dev/null >>"$LOG" 2>&1 &
    exit 0
fi

if ! mkdir "$SESSION" 2>/dev/null; then
    echo 'KindleBoy session directory exists; exit the running session first.' >>"$LOG"
    exit 1
fi
chmod 700 "$SESSION"
RUN="$SESSION/kindleboy"
FROZEN_PIDS=''
STOPPED=''
OLD_GOV=''
CHILD=''
GOV=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
OLD_SAVER="$(lipc-get-prop com.lab126.powerd preventScreenSaver 2>/dev/null)"
case "$OLD_SAVER" in 0|1) ;; *) OLD_SAVER=0 ;; esac

stop_child() {
    if [ -n "$CHILD" ]; then
        kill -TERM "$CHILD" 2>/dev/null
        wait "$CHILD" 2>/dev/null
        CHILD=''
    fi
}
cleanup() {
    trap - 0 INT TERM HUP
    stop_child
    [ -z "$OLD_GOV" ] || echo "$OLD_GOV" >"$GOV" 2>/dev/null
    if [ "$STOPPED" = framework ]; then
        initctl start framework >>"$LOG" 2>&1
    elif [ "$STOPPED" = lab126_gui ]; then
        start lab126_gui >>"$LOG" 2>&1
    fi
    for pid in $FROZEN_PIDS; do kill -CONT "$pid" 2>/dev/null; done
    lipc-set-prop com.lab126.powerd preventScreenSaver "$OLD_SAVER" 2>/dev/null
    rm -f "$RUN"
    rmdir "$SESSION" 2>/dev/null
}
trap cleanup 0
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'exit 129' HUP

# Fail before changing the UI if staging the executable is unsuccessful.
cp "$EXT/bin/kindleboy" "$RUN" || exit 1
chmod 700 "$RUN" || exit 1
export KINDLEBOY_CFG="$EXT/kindleboy.cfg"
echo "=== KindleBoy $(date) mode=${1:-normal} ===" >>"$LOG"
lipc-set-prop com.lab126.powerd preventScreenSaver 1 2>>"$LOG"

if [ -w "$GOV" ]; then
    OLD_GOV="$(cat "$GOV")"
    echo performance >"$GOV" 2>>"$LOG"
fi

if [ "${1:-}" = stopfw ]; then
    if initctl stop framework >>"$LOG" 2>&1; then
        STOPPED=framework
    elif stop lab126_gui >>"$LOG" 2>&1; then
        STOPPED=lab126_gui
    else
        echo 'Could not stop Kindle UI; cancelling dedicated mode.' >>"$LOG"
        exit 1
    fi
    sleep 2
else
    for pid in $(pidof awesome 2>/dev/null); do
        kill -STOP "$pid" 2>/dev/null && FROZEN_PIDS="$FROZEN_PIDS $pid"
    done
fi

"$RUN" >>"$LOG" 2>&1 &
CHILD=$!
wait "$CHILD"
RESULT=$?
CHILD=''
echo "=== KindleBoy exit $RESULT ===" >>"$LOG"
exit "$RESULT"
