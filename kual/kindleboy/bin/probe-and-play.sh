#!/bin/sh
# Legacy browser requests may be retried by the transfer service. Only an
# explicitly armed request may start another session.
REQUEST=/mnt/us/extensions/kindleboy/browser-launch.request
[ -f "$REQUEST" ] || exit 0
rm -f "$REQUEST" || exit 1
sh /mnt/us/extensions/kindleboy/bin/diagnose.sh
exec sh /mnt/us/extensions/kindleboy/bin/run.sh
