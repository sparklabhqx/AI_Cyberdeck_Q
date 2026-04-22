#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ENV_FILE="${PROJECT_ROOT}/config/unoq.env"
REMOTE="${UNOQ_SSH_TARGET:-unoq}"

if [ ! -f "${ENV_FILE}" ]; then
  echo "Missing ${ENV_FILE}"
  exit 1
fi

set -a
# shellcheck disable=SC1090
. "${ENV_FILE}"
set +a

if [ -z "${UNOQ_BLUETOOTH_KEYBOARD_MAC:-}" ]; then
  echo "Set UNOQ_BLUETOOTH_KEYBOARD_MAC in ${ENV_FILE} first."
  exit 1
fi

ssh "${REMOTE}" "bluetoothctl pair ${UNOQ_BLUETOOTH_KEYBOARD_MAC} || true; \
bluetoothctl trust ${UNOQ_BLUETOOTH_KEYBOARD_MAC}; \
bluetoothctl connect ${UNOQ_BLUETOOTH_KEYBOARD_MAC} || true; \
bluetoothctl info ${UNOQ_BLUETOOTH_KEYBOARD_MAC}"
