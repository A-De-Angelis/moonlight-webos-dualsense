#!/bin/sh
# Read-only controller access probe. Run on the LG TV, optionally both as root
# and under the app's UID. This never changes permissions, pairs or writes HID.
printf '%s\n' '=== Moonlight DS5 webOS access probe ==='
printf 'CPU: '; uname -m
printf 'Kernel: '; uname -r
printf 'User: '; id
printf 'HIDRAW devices:\n'
found=0
for node in /dev/hidraw*; do
    [ -e "$node" ] || continue
    found=1
    ls -l "$node"
    if [ -r "$node" ]; then readable=yes; else readable=no; fi
    if [ -w "$node" ]; then writable=yes; else writable=no; fi
    printf '  current user: readable=%s writable=%s\n' "$readable" "$writable"
    base=${node##*/}
    uevent="/sys/class/hidraw/$base/device/uevent"
    if [ -r "$uevent" ]; then
        grep -E '^HID_(ID|NAME)=' "$uevent" 2>/dev/null || :
    fi
done
[ "$found" -eq 1 ] || printf '%s\n' 'No /dev/hidraw* devices detected.'
printf '%s\n' '=== End read-only probe ==='
