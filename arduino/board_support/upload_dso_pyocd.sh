#!/usr/bin/env bash
# =============================================================================
# upload_dso_pyocd.sh
# Arduino IDE Upload Helper for FNIRSI DSO-152 via ST-Link SWD @ 0x08004000
# =============================================================================
set -e

BIN_FILE="$1"

if [ -z "$BIN_FILE" ] || [ ! -f "$BIN_FILE" ]; then
    echo "[DSO-152] Error: Compiled binary not found: $BIN_FILE"
    exit 1
fi

# Ensure initial stack pointer (_estack at offset 0) is within the physical 20 KB SRAM
# (0x20004FF0, below the 0x20004FFC bootloader magic key) even if IDE cached an older boards.txt
if command -v python3 >/dev/null 2>&1; then
    python3 - "$BIN_FILE" <<'PYEOF'
import struct, sys
p = sys.argv[1]
with open(p, "rb") as f:
    data = bytearray(f.read())
if len(data) >= 4:
    sp = struct.unpack_from("<I", data, 0)[0]
    if sp > 0x20004FF0 and (sp & 0x2FF00000) == 0x20000000:
        struct.pack_into("<I", data, 0, 0x20004FF0)
        with open(p, "wb") as f:
            f.write(data)
        print(f"[DSO-152] Clamped vector table _estack from 0x{sp:08X} to 0x20004FF0 (20 KB SRAM)")
PYEOF
fi

echo "============================================================"
echo " FNIRSI DSO-152 ST-Link SWD Uploader (@ 0x08004000)"
echo " Binary : $BIN_FILE ($(stat -c%s "$BIN_FILE" 2>/dev/null || stat -f%z "$BIN_FILE") bytes)"
echo "============================================================"

# 1. Prefer STM32duino's bundled xpack-openocd or system openocd (handles CH32F103 flash cleanly)
OPENOCD_BIN=""
for candidate in "$HOME"/.arduino15/packages/STMicroelectronics/tools/xpack-openocd/*/bin/openocd; do
    if [ -x "$candidate" ]; then
        OPENOCD_BIN="$candidate"
        break
    fi
done
if [ -z "$OPENOCD_BIN" ] && command -v openocd >/dev/null 2>&1; then
    OPENOCD_BIN="$(command -v openocd)"
fi

if [ -n "$OPENOCD_BIN" ]; then
    "$OPENOCD_BIN" -f interface/stlink.cfg -c "set CPUTAPID 0x2ba01477" -f target/stm32f1x.cfg \
        -c "program \"$BIN_FILE\" verify reset exit 0x08004000"
    echo "[+] Flash complete at 0x08004000 (USB bootloader at 0x08000000 preserved)!"
    exit 0
fi

# 2. Fallback to pyOCD with Medium-Density 64KB target (stm32f103c8)
PYOCD_BIN="pyocd"
if ! command -v pyocd >/dev/null 2>&1; then
    if [ -x "$HOME/.local/bin/pyocd" ]; then
        PYOCD_BIN="$HOME/.local/bin/pyocd"
    fi
fi

"$PYOCD_BIN" load -t stm32f103c8 "${BIN_FILE}@0x08004000"
echo "[+] Flash complete at 0x08004000 (USB bootloader at 0x08000000 preserved)!"
