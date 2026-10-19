#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
BIN="$DIR/dso152_bootloader.bin"

echo "========================================================"
echo " FNIRSI DSO-152 Bootloader Flasher"
echo " Target: CH32F103 (Cortex-M3 IDCODE: 0x2ba01477)"
echo " Image:  $BIN (Base 0x08000000)"
echo "========================================================"

if command -v docker >/dev/null 2>&1; then
    echo "[+] Flashing bootloader using OpenOCD container..."
    docker run --rm --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v "$DIR":/fw \
        alpine sh -c "
            apk add --no-cache openocd >/dev/null 2>&1
            openocd -f interface/stlink.cfg -c 'set CPUTAPID 0x2ba01477' -f target/stm32f1x.cfg \
                -c 'program /fw/dso152_bootloader.bin verify reset exit 0x08000000'
        "
    echo "[✓] Bootloader successfully installed at 0x08000000!"
    exit 0
fi

if command -v openocd >/dev/null 2>&1; then
    openocd -f interface/stlink.cfg -c 'set CPUTAPID 0x2ba01477' -f target/stm32f1x.cfg \
        -c "program $BIN verify reset exit 0x08000000"
    echo "[✓] Bootloader successfully installed at 0x08000000!"
    exit 0
fi

echo "[-] No OpenOCD flasher available."
exit 1
