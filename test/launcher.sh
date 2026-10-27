#!/bin/sh
# Run only in a disposable Linux container: /mnt/us is a mock Kindle.
set -eu
mkdir -p /mnt/us/extensions/kindleboy/bin /tmp/kindleboy-mocks
cp /work/kual/kindleboy/bin/run.sh /mnt/us/extensions/kindleboy/bin/run.sh
MOCK=/tmp/kindleboy-mocks
export MOCK_CALLS=/tmp/kindleboy-launcher-calls
cat >"$MOCK/lipc-get-prop" <<'EOF'
#!/bin/sh
echo 1
EOF
cat >"$MOCK/lipc-set-prop" <<'EOF'
#!/bin/sh
echo "lipc $*" >>"$MOCK_CALLS"
EOF
cat >"$MOCK/initctl" <<'EOF'
#!/bin/sh
echo "initctl $*" >>"$MOCK_CALLS"
EOF
chmod +x "$MOCK"/*
export PATH="$MOCK:$PATH"
export KB_DETACHED=1
LAUNCHER=/mnt/us/extensions/kindleboy/bin/run.sh
BIN=/mnt/us/extensions/kindleboy/bin/kindleboy
cat >"$BIN" <<'EOF'
#!/bin/sh
exit 7
EOF
: >"$MOCK_CALLS"
rc=0
sh "$LAUNCHER" stopfw || rc=$?
[ "$rc" -eq 7 ]
[ ! -d /var/tmp/kindleboy-session ]
grep -q 'initctl stop framework' "$MOCK_CALLS"
grep -q 'initctl start framework' "$MOCK_CALLS"
tail -n 1 "$MOCK_CALLS" | grep -q 'preventScreenSaver 1'
echo 'PASS: child failure restores UI, screensaver, and session lock'

cat >"$BIN" <<'EOF'
#!/bin/sh
trap 'exit 0' TERM
while :; do sleep 0.1; done
EOF
: >"$MOCK_CALLS"
sh "$LAUNCHER" &
launcher_pid=$!
sleep 0.3
rc=0
sh "$LAUNCHER" || rc=$?
[ "$rc" -eq 1 ]
kill -TERM "$launcher_pid"
wait "$launcher_pid" || rc=$?
[ "$rc" -eq 143 ]
[ ! -d /var/tmp/kindleboy-session ]
tail -n 1 "$MOCK_CALLS" | grep -q 'preventScreenSaver 1'
echo 'PASS: concurrent launch is rejected and SIGTERM cleans up the child'
