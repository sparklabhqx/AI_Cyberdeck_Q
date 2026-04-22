# Arduino AI Cyberdeck

Reproducible setup for the Arduino UNO Q + Arduino GIGA R1 WiFi + GIGA Display Shield + Waveshare 1.51" transparent OLED build.

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

1. Read [docs/hardware-wiring.md](/Users/flo/arduino-ai-cyberdeck/docs/hardware-wiring.md:1).
2. Read [docs/first-boot.md](/Users/flo/arduino-ai-cyberdeck/docs/first-boot.md:1).
3. Copy `config/unoq.env.example` to `config/unoq.env` and fill it in.
4. Copy `firmware/giga_ai_terminal/giga_config.example.h` to `firmware/giga_ai_terminal/giga_config.h` and fill it in.
5. Run `scripts/setup_unoq.sh`.
6. Flash the GIGA with `scripts/flash_giga.sh`.
7. Build or flash the UNO Q matrix sketch if needed.
8. Run `scripts/verify_system.sh`.

## Fresh Board Requirements

This repo installs the runtime pieces, but a completely fresh UNO Q still needs:

- the correct Zephyr loader flashed first
- a working `llama-cli` binary in `~/bin`
- the required shared libraries in `~/lib`
- a GGUF model in `~/models`

Those steps are documented in [docs/first-boot.md](/Users/flo/arduino-ai-cyberdeck/docs/first-boot.md:1).

## Developer Path

Use [docs/developer-workflow.md](/Users/flo/arduino-ai-cyberdeck/docs/developer-workflow.md:1) if you want to rebuild, modify, or swap parts of the system instead of just reproducing the known-good setup.
