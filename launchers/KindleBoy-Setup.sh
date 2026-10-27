#!/bin/sh
# Name: KindleBoy Setup
# Author: KindleBoy

EXT=/mnt/us/extensions/kindleboy
RESULT=/mnt/us/documents/KindleBoy-Setup-Result.txt
echo 'KindleBoy startup setup...'
if [ -e /mnt/us/DISABLE_KINDLEBOY ]; then
    printf '%s\n' 'BLOCKED' 'DISABLE_KINDLEBOY is present.' 'Remove that USB flag to enable startup.' | tee "$RESULT"
    exit 1
fi
if sh "$EXT/bin/install-autostart.sh" && sh "$EXT/bin/autostart.sh" --mark-current-boot; then
    printf '%s\n' 'SUCCESS' 'KindleBoy autostart enabled.' 'Restart Kindle to test.' | tee "$RESULT"
else
    printf '%s\n' 'FAILED' 'Startup setup did not complete.' 'Connect USB for the setup log.' | tee "$RESULT"
    exit 1
fi
