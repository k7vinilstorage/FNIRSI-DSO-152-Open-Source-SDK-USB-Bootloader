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

# 4. Install boards.local.txt, platform.local.txt, and upload scripts into STM32duino if installed
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
