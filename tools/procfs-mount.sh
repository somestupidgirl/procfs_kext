#!/bin/sh
#
# procfs-mount.sh - mount procfs at ~/proc on login, gated for dev safety.
#
# Run by the per-user LaunchAgent com.beako.procfs.mount. Only mounts when the
# arm flag exists (so the filesystem doesn't auto-mount during development) and
# when not already mounted. Install to /usr/local/sbin/procfs-mount.sh.
#
[ -f /var/db/procfs.enabled ] || exit 0
[ -n "${HOME}" ] || exit 0

# Already mounted?
/sbin/mount | /usr/bin/grep -q " on ${HOME}/proc " && exit 0

/bin/mkdir -p "${HOME}/proc"

# At login the kext may still be loading (boot race), so retry for a while
# until the procfs filesystem accepts the mount.
i=0
while [ "$i" -lt 30 ]; do
    if /sbin/mount -t procfs procfs "${HOME}/proc" 2>/dev/null; then
        echo "procfs-mount: mounted at ${HOME}/proc"
        exit 0
    fi
    i=$((i + 1))
    sleep 1
done

echo "procfs-mount: failed to mount ${HOME}/proc after 30s (is the kext loaded?)" >&2
exit 1
