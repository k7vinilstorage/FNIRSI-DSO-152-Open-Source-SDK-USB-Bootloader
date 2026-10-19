#!/usr/bin/env bash
# =============================================================================
# upload_dso_pyocd.sh
# Arduino IDE Upload Helper for FNIRSI DSO-152 via ST-Link + pyOCD @ 0x08004000
# =============================================================================
set -e

BIN_FILE="$1"

if [ -z "$BIN_FILE" ] || [ ! -f "$BIN_FILE" ]; then
    echo "[DSO-152] Error: Compiled binary not found: $BIN_FILE"
    exit 1
fi

echo "============================================================"
echo " FNIRSI DSO-152 ST-Link SWD Uploader (pyOCD @ 0x08004000)"
echo " Binary : $BIN_FILE"
echo "============================================================"

PYOCD_BIN="pyocd"
if ! command -v pyocd >/dev/null 2>&1; then
    if [ -x "$HOME/.local/bin/pyocd" ]; then
        PYOCD_BIN="$HOME/.local/bin/pyocd"
    fi
fi

"$PYOCD_BIN" load -t stm32f103rc "${BIN_FILE}@0x08004000"
echo "[+] Flash complete at 0x08004000 (USB bootloader at 0x08000000 preserved)!"
