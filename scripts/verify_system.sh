#!/bin/bash
set -euo pipefail

REMOTE="${UNOQ_SSH_TARGET:-unoq}"
BRIDGE_URL="${UNOQ_BRIDGE_URL:-}"
GIGA_PORT="${GIGA_PORT:-$(ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)}"

if [ -z "${BRIDGE_URL}" ]; then
  echo "Set UNOQ_BRIDGE_URL before running this script."
  echo "Example: export UNOQ_BRIDGE_URL=http://unoq.local:8080"
  exit 1
fi

echo "== UNO Q bridge =="
curl -fsS "${BRIDGE_URL}/health"
echo
curl -fsS "${BRIDGE_URL}/model"
echo

echo "== UNO Q services =="
ssh "${REMOTE}" "systemctl --user --no-pager --full status giga-wifi-bridge.service usb-giga-terminal.service | sed -n '1,40p'"

echo
echo "== UNO Q keyboard devices =="
ssh "${REMOTE}" "grep -E 'Name=|Handlers=' /proc/bus/input/devices"

echo
echo "== GIGA USB =="
if [ -n "${GIGA_PORT}" ]; then
  echo "${GIGA_PORT}"
else
  echo "No /dev/cu.usbmodem* port found"
fi
