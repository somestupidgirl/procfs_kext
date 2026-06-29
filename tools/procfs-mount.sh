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

/sbin/mount | /usr/bin/grep -q " on ${HOME}/proc " && exit 0

/bin/mkdir -p "${HOME}/proc"
exec /sbin/mount -t procfs procfs "${HOME}/proc"
