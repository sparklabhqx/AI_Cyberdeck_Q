#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="${PROJECT_ROOT}/firmware/giga_ai_terminal"
CONFIG_TEMPLATE="${SKETCH_DIR}/giga_config.example.h"
CONFIG_FILE="${SKETCH_DIR}/giga_config.h"
FQBN="${GIGA_FQBN:-arduino:mbed_giga:giga}"
PORT="${GIGA_PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"

if [ ! -f "${CONFIG_FILE}" ]; then
  echo "Missing ${CONFIG_FILE}"
  echo "Copy ${CONFIG_TEMPLATE} to ${CONFIG_FILE} and fill in your Wi-Fi and UNO Q host."
  exit 1
fi

if [ -z "${PORT}" ]; then
  echo "No GIGA USB modem port found. Set GIGA_PORT and retry."
  exit 1
fi

arduino-cli compile --fqbn "${FQBN}" "${SKETCH_DIR}"
arduino-cli upload -p "${PORT}" --fqbn "${FQBN}" "${SKETCH_DIR}"
