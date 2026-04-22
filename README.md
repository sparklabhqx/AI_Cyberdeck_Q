# Arduino AI Cyberdeck

Reproducible setup for the Arduino UNO Q + Arduino GIGA R1 WiFi + GIGA Display Shield + Waveshare 1.51" transparent OLED build.

Just paste this into Codex, Claude Code, or whatever coding agent you use to start the setup:
[docs/codex-setup-prompt.md](docs/codex-setup-prompt.md)

This project splits the system into:

- UNO Q Linux side: keyboard capture, local LLM execution, HTTP bridge
- UNO Q MCU side: built-in LED matrix sketch
- GIGA side: Display Shield terminal UI plus transparent OLED model display

## Release Baseline

Known-good local toolchain and board packages from this workspace:

- `arduino-cli`: `1.4.1`
- GIGA board package: `arduino:mbed_giga@4.5.0`
- UNO Q board package: `arduino:zephyr@0.53.1`
- GIGA FQBN: `arduino:mbed_giga:giga`
- UNO Q FQBN: `arduino:zephyr:unoq`

Known-good UNO Q model path in the current setup:

- `/home/arduino/models/qwen2.5-0.5b-instruct-q2_k.gguf`

Known-good bridge endpoint shape:

- `/health`
- `/model`
- `/latest`
- `/push?text=...`

## Project Layout

- `firmware/giga_ai_terminal`: main GIGA sketch
- `firmware/giga_transparent_oled_diag`: OLED truth-source diagnostic
- `firmware/giga_display_diag`: Display Shield diagnostic
- `firmware/unoq_matrix_status`: UNO Q LED matrix sketch
- `unoq`: Python services, start scripts, systemd user units
- `config`: install-specific config templates
- `scripts`: build, flash, install, and verification helpers
- `docs`: wiring, architecture, setup, developer workflow, troubleshooting

## Golden Path

1. Read [docs/hardware-wiring.md](docs/hardware-wiring.md).
2. Read [docs/first-boot.md](docs/first-boot.md).
3. Copy `config/unoq.env.example` to `config/unoq.env` and fill it in.
4. Copy `firmware/giga_ai_terminal/giga_config.example.h` to `firmware/giga_ai_terminal/giga_config.h` and fill it in.
5. Run `scripts/setup_unoq.sh`.
6. Flash the GIGA with `scripts/flash_giga.sh`.
7. Build or flash the UNO Q matrix sketch if needed.
8. Run `scripts/verify_system.sh`.

## Step-By-Step Setup

This section is the shortest practical setup path for a new user with the same hardware on a different Wi-Fi.

### 1. Gather the required information

Before you start, decide or collect:

- your Wi-Fi SSID and password for the GIGA
- the hostname or IP you want to use for the UNO Q bridge
- the SSH target for the UNO Q
- the Bluetooth keyboard MAC address if you want the pairing step scripted
- the GGUF model file you want on the UNO Q

### 2. Assemble and wire the hardware

Use [docs/hardware-wiring.md](docs/hardware-wiring.md).

At minimum, verify:

- the GIGA Display Shield is mounted correctly
- the transparent OLED is wired to `SPI1` on the GIGA
- the `D2` button is wired from `D2` to `GND`
- the UNO Q and GIGA share `GND` if you are using the direct UART path

### 3. Install the required Arduino board packages

This project currently assumes:

- `arduino-cli 1.4.1`
- `arduino:mbed_giga@4.5.0`
- `arduino:zephyr@0.53.1`

Board IDs used by the scripts:

- GIGA: `arduino:mbed_giga:giga`
- UNO Q: `arduino:zephyr:unoq`

### 4. Create the local config files

Create the two install-specific config files:

```bash
cp config/unoq.env.example config/unoq.env
cp firmware/giga_ai_terminal/giga_config.example.h firmware/giga_ai_terminal/giga_config.h
```

Then edit them.

In `config/unoq.env`, fill in:

- `UNOQ_BRIDGE_PORT` if you do not want `8080`
- `UNOQ_MODEL_PATH`
- `UNOQ_KEYBOARD_PREFERRED_NAME`
- `UNOQ_BLUETOOTH_KEYBOARD_ALIAS`
- `UNOQ_BLUETOOTH_KEYBOARD_MAC`

In `firmware/giga_ai_terminal/giga_config.h`, fill in:

- `CYBERDECK_WIFI_SSID`
- `CYBERDECK_WIFI_PASS`
- `CYBERDECK_UNOQ_HOST`
- `CYBERDECK_UNOQ_PORT`

### 5. Prepare the UNO Q

The UNO Q needs both Linux-side runtime setup and MCU-side loader setup.

Linux-side setup:

```bash
./scripts/setup_unoq.sh
```

That installs the Python/runtime dependencies and deploys:

- `giga_wifi_bridge.py`
- `usb_giga_terminal.py`
- the start scripts
- the user `systemd` services

Fresh-board items that are still your responsibility:

- flash the correct Zephyr UNO Q loader
- install `llama-cli` into `~/bin`
- place required shared libraries into `~/lib`
- place your GGUF model into `~/models`

### 6. Pair the Bluetooth keyboard

If you filled in `UNOQ_BLUETOOTH_KEYBOARD_MAC`, run:

```bash
./scripts/pair_keyboard.sh
```

If not, pair it manually on the UNO Q first and then update `config/unoq.env`.

### 7. Flash the GIGA

Connect the GIGA over USB and run:

```bash
./scripts/flash_giga.sh
```

If the modem device is not auto-detected, set:

```bash
export GIGA_PORT=/dev/cu.usbmodemXXXX
```

and retry.

### 8. Build or flash the UNO Q matrix sketch

If you want to build the matrix sketch:

```bash
./scripts/build_unoq_matrix.sh
```

If you want to flash it using the current known-good board-local path:

```bash
./scripts/flash_unoq_matrix_local.sh
```

### 9. Verify the system

Set the bridge URL first:

```bash
export UNOQ_BRIDGE_URL=http://unoq.local:8080
```

Then run:

```bash
./scripts/verify_system.sh
```

Physically confirm:

- the GIGA Display Shield terminal is visible
- the transparent OLED shows the model name
- the `D2` button toggles both displays
- typing on the keyboard appears on the GIGA terminal path

### 10. If something is wrong

Use:

- [docs/troubleshooting.md](docs/troubleshooting.md)
- [docs/first-boot.md](docs/first-boot.md)
- [docs/developer-workflow.md](docs/developer-workflow.md)

## Fresh Board Requirements

This repo installs the runtime pieces, but a completely fresh UNO Q still needs:

- the correct Zephyr loader flashed first
- a working `llama-cli` binary in `~/bin`
- the required shared libraries in `~/lib`
- a GGUF model in `~/models`

Those steps are documented in [docs/first-boot.md](docs/first-boot.md).

## Codex Prompt

If you want another Codex session to set this up for you, use the ready-made prompt in:

- [docs/codex-setup-prompt.md](docs/codex-setup-prompt.md)

That prompt tells Codex to start by asking for the missing config values or SSH target before running the setup.

## Developer Path

Use [docs/developer-workflow.md](docs/developer-workflow.md) if you want to rebuild, modify, or swap parts of the system instead of just reproducing the known-good setup.
