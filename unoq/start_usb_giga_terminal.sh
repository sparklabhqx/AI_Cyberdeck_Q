#!/bin/bash
set -euo pipefail

ENV_FILE="${HOME}/unoq.env"
LOCK_DIR="${HOME}/.cache"
LOCK_FILE="${LOCK_DIR}/usb-giga-terminal.lock"

if [ -f "${ENV_FILE}" ]; then
  set -a
  # shellcheck disable=SC1090
  . "${ENV_FILE}"
  set +a
fi

mkdir -p "${LOCK_DIR}"
export UNOQ_KEYBOARD_PREFERRED_NAME="${UNOQ_KEYBOARD_PREFERRED_NAME:-bluetooth keyboard}"
export UNOQ_BRIDGE_HOST="${UNOQ_BRIDGE_HOST:-127.0.0.1}"
export UNOQ_BRIDGE_PORT="${UNOQ_BRIDGE_PORT:-8080}"

exec /usr/bin/flock "${LOCK_FILE}" python3 "${HOME}/usb_giga_terminal.py"
