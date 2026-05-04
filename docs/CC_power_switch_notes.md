# Power Switch Notes

These notes are for the Hosyond / LCDWIKI / Elecrow-style `3.5" LCD Display ESP32-32E 320x480 Resistance Touch` board used by this project.

## Goal

Add a physical switch that makes the processor and screen stop running while still allowing the battery to charge from the USB-C port.

## Recommended Simple Mod: Hold RESET/EN Low

The safest simple switch point is the onboard `RESET` button. The board documentation maps `RESET` to the ESP32 `EN` reset line, shared with LCD reset.

Wire a maintained SPST switch in parallel with the onboard `RESET` button:

```text
external switch terminal 1 -> one side of RESET button
external switch terminal 2 -> other side of RESET button
```

When the switch is closed, it acts like the reset button is held down. The ESP32 and LCD stay in reset. When the switch is opened, the board boots normally.

Before soldering:

1. Power the board off.
2. Use a multimeter to identify the two electrical sides of the `RESET` button.
3. One side should have continuity to `GND`.
4. The other side is the `EN` / reset node.
5. Solder the external switch across those two sides.

Do not use the `BOOT` button for this. `BOOT` is for firmware download mode, not power control.

## Existing Connectors

The visible onboard connectors do not appear to expose `EN` / `RESET`.

Visible connector labels include:

- `IO35 / IO39`
- `SPEAKER`
- `SPI`: `IO23`, `IO19`, `IO18`, `IO21`, `GND`, `5V`
- `I2C`: `3.3V`, `IO32`, `IO25`, `GND`
- `UART`: `TXD`, `RXD`, `GND`, `5V`
- `BAT`

These connectors are useful for expansion, serial, and battery connection, but they are not the right place for a reset-hold power switch.

## Charging Caveat

Holding `EN` low is not a true zero-current power-off. The board remains electrically powered, and the charger circuit is still active. Published docs list reset current around `40 mA`, so this mode can still drain a battery over time if USB is unplugged.

The benefit is that USB-C charging should still work, because the charger path is separate from the `EN` reset line.

## True Battery Cutoff

A true cutoff would switch the load path from the battery to the board while leaving the charger connected to the battery. On this board that means modifying the battery charge/discharge circuit around the battery connector and P-channel FET/load path, not simply adding a switch in series with the `BAT` connector.

An inline switch on the `BAT` connector is mechanically easy, but when it is off the battery is disconnected from the board's charger too. That means it is not the right choice if the requirement is "off but still charge from USB."

