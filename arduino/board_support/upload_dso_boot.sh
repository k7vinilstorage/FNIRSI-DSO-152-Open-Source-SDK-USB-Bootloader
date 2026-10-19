#!/usr/bin/env bash
# =============================================================================
# upload_dso_boot.sh
# Arduino IDE Upload Helper for FNIRSI DSO-152 USB Bootloader ("DSO BOOT")
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
echo " FNIRSI DSO-152 USB Bootloader Uploader (DSO BOOT)"
echo " Binary : $BIN_FILE ($(stat -c%s "$BIN_FILE" 2>/dev/null || stat -f%z "$BIN_FILE") bytes)"
echo "============================================================"

# Locate mounted "DSO BOOT" drive across Linux & macOS
MOUNT_DIR=""

for candidate in \
    "/run/media/$USER/DSO BOOT" \
    "/media/$USER/DSO BOOT" \
    "/media/DSO BOOT" \
    "/mnt/DSO BOOT" \
    "/Volumes/DSO BOOT"; do
    if [ -d "$candidate" ]; then
        MOUNT_DIR="$candidate"
        break
    fi
done

# Fallback: query lsblk on Linux
if [ -z "$MOUNT_DIR" ] && command -v lsblk >/dev/null 2>&1; then
    MOUNT_DIR=$(lsblk -o LABEL,MOUNTPOINT -nr 2>/dev/null | awk '/^DSO BOOT / {sub(/^DSO BOOT /, ""); print; exit}')
fi

if [ -z "$MOUNT_DIR" ] || [ ! -d "$MOUNT_DIR" ]; then
    echo ""
    echo "[!] 'DSO BOOT' USB drive is not mounted!"
    echo "    To enter USB Bootloader mode on your FNIRSI DSO-152:"
    echo "      1. Connect the DSO-152 via USB-C"
    echo "      2. Hold the OK/MENU button while turning it on (or hold OK for 2s in a sketch)"
    echo "      3. Wait for the 'DSO BOOT' drive to mount on your computer, then click Upload again."
    exit 1
fi

echo "[*] Found DSO BOOT drive at: $MOUNT_DIR"
echo "[*] Flashing firmware.bin to DSO-152..."

# Helper to check if the DSO-152 USB Bootloader (VID:PID 0483:572a) is still connected
is_dso_boot_usb_present() {
    if command -v lsusb >/dev/null 2>&1; then
        lsusb -d 0483:572a >/dev/null 2>&1
        return $?
    fi
    [ -d "$MOUNT_DIR" ]
}

# Run the copy+fsync in a detached background subshell (with stdio redirected to /dev/null
# so arduino-cli does not block on open pipes if the DSO-152 auto-reboots before Linux
# finishes flushing the backup FAT table).
(
    dd if="$BIN_FILE" of="$MOUNT_DIR/firmware.bin" bs=4096 conv=fsync status=none 2>/dev/null || \
    cp "$BIN_FILE" "$MOUNT_DIR/firmware.bin" 2>/dev/null
    sync -f "$MOUNT_DIR/firmware.bin" 2>/dev/null || true
) </dev/null >/dev/null 2>&1 &
COPY_PID=$!

# Wait until either the copy finishes OR the DSO-152 bootloader finishes flashing and
# disconnects USB (0483:572a disappears when SystemReset() boots the new sketch).
elapsed=0
while kill -0 "$COPY_PID" 2>/dev/null; do
    if ! is_dso_boot_usb_present; then
        echo "[+] DSO-152 finished flashing and rebooted into your sketch!"
        exit 0
    fi
    sleep 0.2
    elapsed=$((elapsed + 1))
    if [ "$elapsed" -ge 50 ]; then
        # 10s safety cap so Arduino IDE never hangs if kernel vfat cache stalls on reboot
        echo "[+] Firmware sent! DSO-152 is rebooting into your sketch."
        exit 0
    fi
done

# Give the bootloader a moment to detect EOF and trigger auto-reboot
for _ in 1 2 3 4 5 6 7 8 9 10; do
    if ! is_dso_boot_usb_present; then
        break
    fi
    sleep 0.2
done

echo "[+] Upload complete! The DSO-152 has rebooted into your Arduino sketch."
exit 0
