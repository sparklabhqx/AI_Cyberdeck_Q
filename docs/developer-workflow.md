# Developer Workflow

## Goals

Use this path if you are changing behavior, rebuilding pieces, or diagnosing regressions instead of just reproducing the current working setup.

## GIGA Work

Main sketch:

- `firmware/giga_ai_terminal/giga_ai_terminal.ino`

Build:

```bash
./scripts/build_giga.sh
```

Flash:

```bash
./scripts/flash_giga.sh
```

Notes:

- `giga_config.h` is intentionally local and install-specific
- do not edit credentials directly into the sketch again

## UNO Q Linux Work

Runtime files:

- `unoq/giga_wifi_bridge.py`
- `unoq/usb_giga_terminal.py`
- `unoq/start-giga-wifi-bridge.sh`
- `unoq/start_usb_giga_terminal.sh`

Deploy updated runtime:

```bash
./scripts/install_unoq_services.sh
```

## UNO Q Matrix Work

Sketch:

- `firmware/unoq_matrix_status/unoq-matrix-status.ino`

Build:

```bash
./scripts/build_unoq_matrix.sh
```

Flash by the current known-good board-local path:

```bash
./scripts/flash_unoq_matrix_local.sh
```

## Diagnostics

Use these sketches as truth sources before blaming the integrated sketch:

- transparent OLED: `firmware/giga_transparent_oled_diag`
- Display Shield: `firmware/giga_display_diag`

Use these checks before changing code:

- `curl http://UNOQ:8080/health`
- `curl http://UNOQ:8080/model`
- `ssh unoq 'systemctl --user status giga-wifi-bridge.service usb-giga-terminal.service'`

## Loader Rule

For UNO Q MCU work:

- if a flashed sketch never visibly starts
- and the boot animation still appears
- debug the Zephyr loader path first

Do not assume the matrix art is the first problem.
