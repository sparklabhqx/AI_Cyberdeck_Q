# Hardware Wiring

## Boards

- Arduino UNO Q
- Arduino GIGA R1 WiFi
- Arduino GIGA Display Shield
- Waveshare 1.51" transparent OLED
- momentary push button
- Bluetooth keyboard

## GIGA Display Shield

Mount the GIGA Display Shield directly on the GIGA.

## UNO Q to GIGA UART

Known UART demo wiring from the project notes:

- UNO Q `D1 / TX` -> GIGA `D19 / RX1`
- UNO Q `GND` -> GIGA `GND`

If you want two-way UART later:

- UNO Q `RX` -> GIGA `TX1`

The main project currently uses the UNO Q HTTP bridge over Wi‑Fi for the text path, but the direct UART wiring is still useful for diagnostics and fallback demos.

## Transparent OLED on GIGA

The transparent OLED uses `SPI1` on the GIGA, not the default `SPI` bus.

Keep this wiring:

- `VCC -> 3V3`
- `GND -> GND`
- `DIN -> D11`
- `CLK -> D13`
- `CS -> D4`
- `DC -> D5`
- `RST -> D6`

Important bus fact:

- GIGA default `SPI` is on `D90/D91`
- visible header `D11/D13` are `SPI1`

## Display Toggle Button

Current known-good wiring:

- one side of the button -> `D2`
- other side of the button -> `GND`

Current sketch assumption:

- `INPUT_PULLUP`
- idle = `HIGH`
- pressed = `LOW`

## Bluetooth Keyboard

The keyboard pairs to the UNO Q Linux side, not to the GIGA.

The runtime prefers a Bluetooth keyboard over USB receiver devices when both are present.
