# First Boot

This is the shortest path for somebody with the same hardware on a different Wi‑Fi and fresh boards.

## 1. Install Local Arduino Tooling

Install and verify:

- `arduino-cli`
- `arduino:mbed_giga@4.5.0`
- `arduino:zephyr@0.53.1`

Local board IDs used by this project:

- GIGA: `arduino:mbed_giga:giga`
- UNO Q: `arduino:zephyr:unoq`

## 2. Prepare Local Config

Create:

- `config/unoq.env` from `config/unoq.env.example`
- `firmware/giga_ai_terminal/giga_config.h` from `firmware/giga_ai_terminal/giga_config.example.h`

Fill in:

- new Wi‑Fi SSID and password
- UNO Q bridge IP or hostname
- Bluetooth keyboard MAC and alias if known
- model path if different

## 3. Flash the UNO Q Loader

This must be treated as a pinned prerequisite.

Use the same Zephyr package release that you compile sketches with:

- current pinned package: `arduino:zephyr@0.53.1`

If the UNO Q matrix sketch appears to flash but never starts, assume loader/sketch mismatch before assuming the matrix bitmap code is wrong.

## 4. Provision UNO Q Linux Side

Run:

```bash
./scripts/setup_unoq.sh
```

That installs packages, copies the runtime files, installs the systemd user services, and starts them.

## 5. Install LLM Runtime on UNO Q

You still need:

- `~/bin/llama-cli`
- required shared libraries in `~/lib`
- a model in `~/models`

Current known-good model:

- `qwen2.5-0.5b-instruct-q2_k.gguf`

## 6. Pair the Keyboard

Edit `config/unoq.env` and set:

- `UNOQ_BLUETOOTH_KEYBOARD_MAC`

Then run:

```bash
./scripts/pair_keyboard.sh
```

## 7. Flash the GIGA

Run:

```bash
./scripts/flash_giga.sh
```

## 8. Verify

Run:

```bash
./scripts/verify_system.sh
```

Then physically confirm:

- GIGA Display Shield shows the terminal
- transparent OLED shows the model label
- `D2` toggles both displays
- keyboard input appears on the GIGA path
