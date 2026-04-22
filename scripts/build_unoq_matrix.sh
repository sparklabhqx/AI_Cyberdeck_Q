#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SKETCH_DIR="${PROJECT_ROOT}/firmware/unoq_matrix_status"
FQBN="${UNOQ_FQBN:-arduino:zephyr:unoq}"

arduino-cli compile --fqbn "${FQBN}" "${SKETCH_DIR}"
