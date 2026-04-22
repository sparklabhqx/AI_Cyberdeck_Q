# Architecture

## High-Level Roles

### UNO Q Linux side

Runs:

- `giga_wifi_bridge.py`
- `usb_giga_terminal.py`
- `llama-cli`

Responsibilities:

- capture keyboard input from Linux input devices
- run the local GGUF model
- format terminal output as `Prompt>>` / `QWEN>>`
- expose the latest screen payload to the GIGA over HTTP

### UNO Q MCU side

Runs a Zephyr-loaded sketch for the built-in LED matrix.

Current sketch:

- `firmware/unoq_matrix_status/unoq-matrix-status.ino`

Important detail:

- UNO Q sketches are loaded by the Zephyr loader
- they are not flashed as a traditional standalone Arduino app
- loader/version mismatch can leave the matrix dark even if flashing reports success

### GIGA side

Runs:

- `firmware/giga_ai_terminal/giga_ai_terminal.ino`

Responsibilities:

- connect to local Wi‑Fi
- poll UNO Q `/latest`
- render the terminal UI on the Display Shield
- colorize prefixed prompt and response lines
- drive the transparent OLED over `SPI1`
- toggle both displays with the `D2` button

## Data Flow

1. Keyboard events arrive on the UNO Q Linux side.
2. `usb_giga_terminal.py` updates the terminal state and runs the local model.
3. The rendered terminal text is pushed into `giga_wifi_bridge.py`.
4. The GIGA polls `/latest` on the UNO Q over Wi‑Fi.
5. The GIGA renders the resulting lines on the Display Shield.
6. The GIGA separately polls `/model` to show the active model name on the transparent OLED.

## Terminal Protocol

Current intended format:

```text
Prompt>> text input prompt
-----------------------------
QWEN>> llm answer text
```

Color rules in the GIGA sketch:

- `Prompt>>` prefix: blue
- prompt body: white
- separator: blue
- `QWEN>>` prefix: blue
- response body: blue
