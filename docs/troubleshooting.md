# Troubleshooting

## GIGA has no Wi‑Fi bridge data

Check:

- `curl http://UNOQ_IP:8080/health`
- `curl http://UNOQ_IP:8080/model`
- GIGA serial heartbeat on USB

If the GIGA heartbeat says `BRIDGE WAIT`, fix the UNO Q side first.

## GIGA USB port changed

The modem device name can move between reconnects and reflashes.

Check:

```bash
ls /dev/cu.usbmodem*
```

Set `GIGA_PORT` explicitly before flashing if needed.

## Transparent OLED is dark

First prove the hardware path with:

- `firmware/giga_transparent_oled_diag/giga_transparent_oled_diag.ino`

Re-check:

- `SPI1`
- `SPI_MODE3`
- `D11/D13/D4/D5/D6` wiring

## D2 toggle does not work

Re-check the simple wiring:

- `D2 -> button -> GND`

Do not add polarity auto-detection logic first.

The current working assumption is:

- `INPUT_PULLUP`
- pressed = `LOW`

## UNO Q bridge is healthy but keyboard input does nothing

Check:

- `bluetoothctl info <MAC>`
- `/proc/bus/input/devices`

Common failure mode:

- keyboard is paired and trusted but not currently connected

## UNO Q matrix is dark after flash

Treat this as a loader/startup issue first.

Check:

- UNO Q Zephyr package version
- loader version on the board
- sketch artifact type
- debug vs standard mode

Only after the sketch is clearly starting should you return to matrix animation logic.
