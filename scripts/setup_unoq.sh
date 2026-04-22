#!/bin/bash
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REMOTE="${UNOQ_SSH_TARGET:-unoq}"
REMOTE_HOME="${UNOQ_REMOTE_HOME:-/home/arduino}"

ssh "${REMOTE}" "sudo apt update && sudo apt install -y \
  python3 python3-pip curl wget git build-essential cmake bluez socat"

ssh "${REMOTE}" "mkdir -p ${REMOTE_HOME}/bin ${REMOTE_HOME}/lib ${REMOTE_HOME}/models ${REMOTE_HOME}/.cache ${REMOTE_HOME}/.config/systemd/user"

"${PROJECT_ROOT}/scripts/install_unoq_services.sh"

echo
echo "UNO Q base setup complete."
echo "If this is a fresh board, still do these separately:"
echo "  1. install the Zephyr UNO Q loader version you want to standardize on"
echo "  2. copy a GGUF model into ${REMOTE_HOME}/models"
echo "  3. install llama-cli and its shared libraries into ${REMOTE_HOME}/bin and ${REMOTE_HOME}/lib"
