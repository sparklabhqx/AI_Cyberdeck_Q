#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="${PROJECT_ROOT}/firmware/giga_ai_terminal"
CONFIG_TEMPLATE="${SKETCH_DIR}/giga_config.example.h"
CONFIG_FILE="${SKETCH_DIR}/giga_config.h"
FQBN="${GIGA_FQBN:-arduino:mbed_giga:giga}"

if [ ! -f "${CONFIG_FILE}" ]; then
  echo "Missing ${CONFIG_FILE}"
  echo "Copy ${CONFIG_TEMPLATE} to ${CONFIG_FILE} and fill in your Wi-Fi and UNO Q host."
  exit 1
fi

arduino-cli compile --fqbn "${FQBN}" "${SKETCH_DIR}"
