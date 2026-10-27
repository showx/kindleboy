#!/bin/sh
# Collect only system launch metadata and this application's own index entries.
# No system settings or databases are modified.
OUT=/mnt/us/kindleboy-diagnostics.txt
TMP=/mnt/us/kindleboy-diagnostics.txt.tmp
trap 'rm -f "$TMP"' 0
{
    echo 'KindleBoy diagnostic report v1'
    date -u
    uname -a
    id
    echo '\n=== Memory and storage ==='
    head -n 12 /proc/meminfo
    df -k / /var/local /mnt/us

    echo '\n=== Scriptlet components ==='
    for path in \
        /var/local/kmc/bin/sh_integration_launcher \
        /var/local/kmc/lib/sh_integration_extractor.so \
        /usr/lib/ccat/sh_integration_extractor.so \
        /usr/bin/logThis.sh \
        /etc/upstart/framework.conf \
        /etc/upstart/kmc.conf; do
        ls -ld "$path"
    done
    echo '\n=== Application registration (shell integration only) ==='
    if [ -f /var/local/appreg.db ]; then
        sqlite3 /var/local/appreg.db "PRAGMA query_only=ON; SELECT ext,mimetype FROM mimetypes WHERE ext='sh'; SELECT handlerId,name,value FROM properties WHERE handlerId LIKE '%shell_integration%'; SELECT interface,handlerId,contentId,defaultAssoc FROM associations WHERE handlerId LIKE '%shell_integration%';"
    fi
    echo '\n=== Own scriptlet headers ==='
    for path in /mnt/us/documents/KindleBoy.sh /mnt/us/documents/KindleBoy-Dedicated.sh; do
        ls -l "$path"
        head -n 12 "$path"
    done
    echo '\n=== Own library entries ==='
    if [ -f /var/local/cc.db ]; then
        sqlite3 /var/local/cc.db "PRAGMA query_only=ON; SELECT p_location FROM Entries WHERE p_location LIKE '%KindleBoy%';"
    fi
    echo '\n=== Framework and startup configuration ==='
    initctl status framework
    lipc-get-prop com.lab126.volumd driveModeState
    for path in /etc/upstart/framework.conf /etc/upstart/kmc.conf; do
        echo "$path"
        [ ! -f "$path" ] || head -n 160 "$path"
    done
    echo '\nstatus=complete'
} >"$TMP" 2>&1
mv -f "$TMP" "$OUT"
