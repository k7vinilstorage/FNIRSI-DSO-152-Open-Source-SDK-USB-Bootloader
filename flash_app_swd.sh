#!/usr/bin/env bash
# Flash application binary via SWD to address 0x08004000
set -e

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_binary.bin>"
    echo "Example: $0 examples/02_breakout_game/build/breakout_game.bin"
    exit 1
fi

BIN="$(realpath "$1")"
if [ ! -f "$BIN" ]; then
    echo "[-] File not found: $BIN"
    exit 1
fi

echo "========================================================"
echo " FNIRSI DSO-152 SWD Application Flasher"
echo " Target: CH32F103 (Cortex-M3 IDCODE: 0x2ba01477)"
echo " Image:  $BIN (Base 0x08004000)"
echo "========================================================"

if command -v docker >/dev/null 2>&1; then
    echo "[+] Flashing application using Docker OpenOCD..."
    DIR="$(dirname "$BIN")"
    NAME="$(basename "$BIN")"
    docker run --rm --privileged \
        -v /dev/bus/usb:/dev/bus/usb \
        -v "$DIR":/fw \
        alpine sh -c "
            apk add --no-cache openocd >/dev/null 2>&1
            openocd -f interface/stlink.cfg -c 'set CPUTAPID 0x2ba01477' -f target/stm32f1x.cfg \
                -c 'program /fw/$NAME verify reset exit 0x08004000'
        "
    echo "[✓] Application flashed successfully to 0x08004000!"
    exit 0
fi

if command -v openocd >/dev/null 2>&1; then
    openocd -f interface/stlink.cfg -c 'set CPUTAPID 0x2ba01477' -f target/stm32f1x.cfg \
        -c "program $BIN verify reset exit 0x08004000"
    echo "[✓] Application flashed successfully to 0x08004000!"
    exit 0
fi

echo "[-] No OpenOCD flasher available."
exit 1
