#!/usr/bin/env bash
#
# Build and install procfs from a clean tree.
#
# Usage: ./install.sh [make args...] [load]
#
#   make args   Passed through to make, e.g. ARCH=universal
#   load        After installing, load the kext into the running kernel
#               (sudo kmutil load). Omit to load manually or on reboot.
#
# Runs, in order:
#   make clean            # remove previous build artifacts (no sudo)
#   make [args]           # build kext + fs + tools + plists into ./bin (no sudo)
#   sudo make install     # install everything; prompts for your password
#
# `make install` itself unmounts/unloads/clears-staging for any previously
# installed build (see the preinstall target), so this script does not.
#
set -euo pipefail
cd "$(dirname "$0")"

# Split the optional "load" keyword out of the make arguments.
DO_LOAD=0
MAKE_ARGS=()
for arg in "$@"; do
	if [ "$arg" = "load" ]; then
		DO_LOAD=1
	else
		MAKE_ARGS+=("$arg")
	fi
done

echo "==> make clean"
make clean

echo "==> make ${MAKE_ARGS[*]}"
make "${MAKE_ARGS[@]}"

echo "==> sudo make install  (you may be prompted for your password)"
sudo make install "${MAKE_ARGS[@]}"

if [ "$DO_LOAD" -eq 1 ]; then
	echo "==> Loading procfs"
	if sudo kmutil load -p /Library/Extensions/procfs.kext; then
		kextstat | grep "com.beako.filesystems.procfs" || true
	else
		echo "    load failed; a reboot may be required to load from the AuxKC."
	fi
else
	echo "==> procfs installed. To arm auto-load + auto-mount on boot:"
	echo "      sudo touch /var/db/procfs.enabled && sudo reboot"
	echo "    Or load now without rebooting:"
	echo "      sudo kmutil load -p /Library/Extensions/procfs.kext"
fi
