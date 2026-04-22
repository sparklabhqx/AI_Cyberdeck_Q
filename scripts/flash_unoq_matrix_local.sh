#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE="${UNOQ_SSH_TARGET:-unoq}"
REMOTE_HOME="${UNOQ_REMOTE_HOME:-/home/arduino}"
FQBN="${UNOQ_FQBN:-arduino:zephyr:unoq}"
SKETCH_DIR="${PROJECT_ROOT}/firmware/unoq_matrix_status"
CACHE_ROOT="${HOME}/Library/Caches/arduino/sketches"

arduino-cli compile --fqbn "${FQBN}" "${SKETCH_DIR}"

ARTIFACT="$(find "${CACHE_ROOT}" -name 'unoq-matrix-status.ino.elf-zsk.bin' | sort | tail -n 1)"

if [ -z "${ARTIFACT}" ]; then
  echo "Could not find compiled UNO Q sketch artifact."
  exit 1
fi

scp "${ARTIFACT}" "${REMOTE}:${REMOTE_HOME}/unoq-matrix-status.ino.elf-zsk.bin"
ssh "${REMOTE}" "/opt/openocd/bin/arduino-flash.sh ${REMOTE_HOME}/unoq-matrix-status.ino.elf-zsk.bin"
