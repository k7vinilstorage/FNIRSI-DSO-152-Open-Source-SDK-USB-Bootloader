#!/usr/bin/env bash
# =============================================================================
# install_arduino_ide.sh
# Installs the FNIRSI_DSO152 Arduino Library and STM32duino Board/Upload rules
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARDUINO_LIB_DIR="${HOME}/Arduino/libraries"

echo "============================================================"
echo " FNIRSI DSO-152 Arduino IDE Installer"
echo "============================================================"

# 1. Install FNIRSI_DSO152 library to ~/Arduino/libraries/
mkdir -p "${ARDUINO_LIB_DIR}"
rm -rf "${ARDUINO_LIB_DIR}/FNIRSI_DSO152"
cp -r "${SCRIPT_DIR}/FNIRSI_DSO152" "${ARDUINO_LIB_DIR}/FNIRSI_DSO152"
echo "[+] Installed library to: ${ARDUINO_LIB_DIR}/FNIRSI_DSO152"

# 2. Generate FNIRSI_DSO152.zip for Arduino IDE "Add .ZIP Library..."
if command -v zip >/dev/null 2>&1; then
    (cd "${SCRIPT_DIR}" && rm -f FNIRSI_DSO152.zip && zip -rq FNIRSI_DSO152.zip FNIRSI_DSO152)
    echo "[+] Created ZIP archive : ${SCRIPT_DIR}/FNIRSI_DSO152.zip"
elif command -v python3 >/dev/null 2>&1; then
    python3 -c "import shutil; shutil.make_archive('${SCRIPT_DIR}/FNIRSI_DSO152', 'zip', '${SCRIPT_DIR}', 'FNIRSI_DSO152')"
    echo "[+] Created ZIP archive : ${SCRIPT_DIR}/FNIRSI_DSO152.zip"
fi

# 3. Pre-configure STM32duino Boards Manager URL in Arduino IDE 2.x (~/.arduinoIDE/arduino-cli.yaml)
ARDUINO_CLI_YAML="${HOME}/.arduinoIDE/arduino-cli.yaml"
STM32_INDEX_URL="https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json"
if [ -f "${ARDUINO_CLI_YAML}" ]; then
    if ! grep -q "package_stmicroelectronics_index.json" "${ARDUINO_CLI_YAML}"; then
        cat > "${ARDUINO_CLI_YAML}" <<EOF
board_manager:
    additional_urls:
        - ${STM32_INDEX_URL}
EOF
        echo "[+] Added STM32duino Boards Manager URL to: ${ARDUINO_CLI_YAML}"
    else
        echo "[+] STM32duino Boards Manager URL already present in: ${ARDUINO_CLI_YAML}"
    fi
fi

# 4. Install boards.local.txt, platform.local.txt, upload scripts, and patch boards.txt top entries
chmod +x "${SCRIPT_DIR}/board_support/upload_dso_boot.sh" "${SCRIPT_DIR}/board_support/upload_dso_pyocd.sh"

STM32_HW_ROOT="${HOME}/.arduino15/packages/STMicroelectronics/hardware/stm32"
if [ -d "${STM32_HW_ROOT}" ]; then
    for ver_dir in "${STM32_HW_ROOT}"/*; do
        if [ -d "${ver_dir}" ]; then
            cp "${SCRIPT_DIR}/board_support/boards.local.txt" "${ver_dir}/boards.local.txt"
            cp "${SCRIPT_DIR}/board_support/platform.local.txt" "${ver_dir}/platform.local.txt"
            cp "${SCRIPT_DIR}/board_support/upload_dso_boot.sh" "${ver_dir}/upload_dso_boot.sh"
            cp "${SCRIPT_DIR}/board_support/upload_dso_pyocd.sh" "${ver_dir}/upload_dso_pyocd.sh"
            chmod +x "${ver_dir}/upload_dso_boot.sh" "${ver_dir}/upload_dso_pyocd.sh"

            # Also insert FNIRSI_DSO152 at the VERY TOP of GenF1.menu.pnum (above BlackPill F103C8)
            # and at the VERY TOP of GenF1.menu.upload_method in boards.txt so it is #1 in the menu!
            if [ -f "${ver_dir}/boards.txt" ] && ! grep -q "GenF1.menu.pnum.FNIRSI_DSO152=" "${ver_dir}/boards.txt"; then
                python3 - "${ver_dir}/boards.txt" <<'PYEOF'
import sys

path = sys.argv[1]
with open(path, "r", encoding="utf-8") as f:
    text = f.read()

pnum_block = """# FNIRSI_DSO152 board (Top of list)
GenF1.menu.pnum.FNIRSI_DSO152=FNIRSI DSO-152 (AT32F403A / STM32F103RC)
GenF1.menu.pnum.FNIRSI_DSO152.upload.maximum_size=245760
GenF1.menu.pnum.FNIRSI_DSO152.upload.maximum_data_size=49152
GenF1.menu.pnum.FNIRSI_DSO152.build.board=GENERIC_F103RCTX
GenF1.menu.pnum.FNIRSI_DSO152.build.product_line=STM32F103xE
GenF1.menu.pnum.FNIRSI_DSO152.build.variant=STM32F1xx/F103R(C-D-E)T
GenF1.menu.pnum.FNIRSI_DSO152.build.flash_offset=0x4000
GenF1.menu.pnum.FNIRSI_DSO152.debug.svd_file={runtime.tools.STM32_SVD.path}/svd/STM32F1xx/STM32F103.svd

"""

upload_block = """GenF1.menu.upload_method.dso152_usbMethod=FNIRSI DSO-152 USB Drive (DSO BOOT @ 0x08004000)
GenF1.menu.upload_method.dso152_usbMethod.upload.protocol=dso152_usb
GenF1.menu.upload_method.dso152_usbMethod.upload.tool=dso152_usb
GenF1.menu.upload_method.dso152_usbMethod.upload.tool.default=dso152_usb
GenF1.menu.upload_method.dso152_usbMethod.build.flash_offset=0x4000

GenF1.menu.upload_method.dso152_pyocdMethod=FNIRSI DSO-152 ST-Link (pyOCD @ 0x08004000)
GenF1.menu.upload_method.dso152_pyocdMethod.upload.protocol=dso152_pyocd
GenF1.menu.upload_method.dso152_pyocdMethod.upload.tool=dso152_pyocd
GenF1.menu.upload_method.dso152_pyocdMethod.upload.tool.default=dso152_pyocd
GenF1.menu.upload_method.dso152_pyocdMethod.build.flash_offset=0x4000

"""

text = text.replace("# BLACKPILL_F103C8 board\n", pnum_block + "# BLACKPILL_F103C8 board\n", 1)
text = text.replace("GenF1.menu.upload_method.swdMethod=", upload_block + "GenF1.menu.upload_method.swdMethod=", 1)

with open(path, "w", encoding="utf-8") as f:
    f.write(text)
PYEOF
                echo "[+] Patched ${ver_dir}/boards.txt to place FNIRSI DSO-152 at the VERY TOP of Board part number & Upload method!"
            fi
            echo "[+] Installed DSO-152 Board & Upload support into: ${ver_dir}"
        fi
    done
else
    echo "[i] STM32duino core not yet installed in ${STM32_HW_ROOT}."
    echo "    In Arduino IDE, open Tools -> Board -> Boards Manager, install 'STM32 MCU based boards',"
    echo "    then re-run ./arduino/install_arduino_ide.sh to enable one-click 'DSO BOOT' upload!"
fi

echo ""
echo "Done! Restart the Arduino IDE and check:"
echo "  File -> Examples -> FNIRSI DSO-152"
