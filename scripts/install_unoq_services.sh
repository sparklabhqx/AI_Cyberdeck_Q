#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE="${UNOQ_SSH_TARGET:-unoq}"
REMOTE_HOME="${UNOQ_REMOTE_HOME:-/home/arduino}"
LOCAL_ENV_FILE="${PROJECT_ROOT}/config/unoq.env"
LOCAL_ENV_TEMPLATE="${PROJECT_ROOT}/config/unoq.env.example"

if [ ! -f "${LOCAL_ENV_FILE}" ]; then
  echo "Missing ${LOCAL_ENV_FILE}"
  echo "Copy ${LOCAL_ENV_TEMPLATE} to ${LOCAL_ENV_FILE} and fill in the local model and keyboard settings."
  exit 1
fi

ssh "${REMOTE}" "mkdir -p ${REMOTE_HOME}/.config/systemd/user"

scp "${PROJECT_ROOT}/unoq/giga_wifi_bridge.py" "${REMOTE}:${REMOTE_HOME}/giga_wifi_bridge.py"
scp "${PROJECT_ROOT}/unoq/usb_giga_terminal.py" "${REMOTE}:${REMOTE_HOME}/usb_giga_terminal.py"
scp "${PROJECT_ROOT}/unoq/start-giga-wifi-bridge.sh" "${REMOTE}:${REMOTE_HOME}/start-giga-wifi-bridge.sh"
scp "${PROJECT_ROOT}/unoq/start_usb_giga_terminal.sh" "${REMOTE}:${REMOTE_HOME}/start_usb_giga_terminal.sh"
scp "${PROJECT_ROOT}/unoq/systemd/giga-wifi-bridge.service" "${REMOTE}:${REMOTE_HOME}/.config/systemd/user/giga-wifi-bridge.service"
scp "${PROJECT_ROOT}/unoq/systemd/usb-giga-terminal.service" "${REMOTE}:${REMOTE_HOME}/.config/systemd/user/usb-giga-terminal.service"
scp "${LOCAL_ENV_FILE}" "${REMOTE}:${REMOTE_HOME}/unoq.env"

ssh "${REMOTE}" "chmod +x \
  ${REMOTE_HOME}/giga_wifi_bridge.py \
  ${REMOTE_HOME}/usb_giga_terminal.py \
  ${REMOTE_HOME}/start-giga-wifi-bridge.sh \
  ${REMOTE_HOME}/start_usb_giga_terminal.sh && \
  systemctl --user daemon-reload && \
  systemctl --user enable giga-wifi-bridge.service usb-giga-terminal.service && \
  systemctl --user restart giga-wifi-bridge.service usb-giga-terminal.service && \
  systemctl --user --no-pager --full status giga-wifi-bridge.service usb-giga-terminal.service"
