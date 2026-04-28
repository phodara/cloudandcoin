# Charging Monitor Wiring

These notes are for the Hosyond / LCDWIKI / Elecrow-style `3.5" LCD Display ESP32-32E 320x480 Resistance Touch` board used by this project.

## Goal

Add a reliable on-screen charging indicator by wiring the battery charger status signal to an ESP32 input.

The board already charges from USB-C. This mod only gives the ESP32 firmware a readable signal that says whether the battery charger IC is actively charging.

## Recommended Signal

Use the TP4054 battery charger IC `CHRG` pin.

The published schematic shows:

- Charger IC: `TP4054`
- Charger status pin: `CHRG`
- Battery connector: `BAT+` / `GND`
- Battery ADC divider: `BAT_ADC`

The `CHRG` signal is the best signal for this feature because it indicates actual charging, not just USB power being plugged in.

## Recommended ESP32 Input

Use `IO35`.

Reasons:

- `IO35` is exposed on the board.
- `IO35` is input-only, which is ideal for a monitor signal.
- It is not used by the current firmware.

Do not use a display, touch, SD card, UART, or bootstrapping pin for this.

## Pull-Up Requirement

The TP4054 `CHRG` output is normally open-drain and active-low:

```text
LOW  = charging
HIGH = not charging, full, no battery, or no USB power
```

`IO35` does not have an internal pull-up resistor, so add an external pull-up:

```text
3.3V -> 10k resistor -> IO35
CHRG -> IO35
```

Because this is all on the same board, ground is already shared.

## Wiring Summary

With the board powered off:

```text
TP4054 CHRG pin -> IO35
3.3V -> 10k resistor -> IO35
```

After wiring:

```text
IO35 LOW  -> battery is charging
IO35 HIGH -> battery is not actively charging
```

## Connector Notes

The board exposes useful connector pins, including:

- `IO35 / IO39`
- `I2C`: `3.3V`, `IO32`, `IO25`, `GND`
- `SPI`: includes `GND` and `5V`
- `UART`: includes `GND` and `5V`
- `BAT`

Use the exposed `IO35` connector for the ESP32 input. Use an exposed `3.3V` pin for the pull-up resistor.

Do not connect `5V` directly to any ESP32 GPIO.

## Software Configuration

Once the hardware mod is installed, the firmware should enable an active-low charge monitor on `IO35`.

Suggested configuration:

```cpp
#define USB_CHARGE_DETECT_PIN  35
#define USB_CHARGE_ACTIVE_LOW  1
```

Suggested setup:

```cpp
pinMode(USB_CHARGE_DETECT_PIN, INPUT);
```

Suggested read logic:

```cpp
bool charging = digitalRead(USB_CHARGE_DETECT_PIN) == LOW;
```

The UI can then show a green lightning bolt when `charging` is true.

## Behavior Caveats

This detects active battery charging, not merely USB plugged in.

That means:

- USB plugged in and battery charging: indicator on.
- USB plugged in and battery full: indicator may be off.
- USB unplugged: indicator off.
- Battery missing or charger inactive: indicator off.

This is usually the most honest behavior for a charging indicator.

## Safer Alternatives

If the goal is to show USB power present instead of actual charging, use a voltage divider from the board's `5V` rail into an input-only GPIO such as `IO35` or `IO39`.

That is a different signal:

```text
USB 5V present -> USB plugged in
```

It does not prove the battery is actively charging.
