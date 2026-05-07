# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build and Upload

```bash
pio run                          # compile
pio run -t upload                # compile and flash
pio device monitor -b 115200     # open serial monitor
pio run -t upload && pio device monitor -b 115200   # flash then monitor
```

Upload and monitor target port: `/dev/cu.usbserial-10` (override in `platformio.ini` if the ESP32 appears on a different port).

To disable a feature at compile time without editing source, pass a flag in `platformio.ini`:

```ini
build_flags = ... -D ENABLE_PRESENCE_SLEEP=0
```

## Architecture

The entire firmware compiles as **one C++ translation unit**. `src/main.cpp` declares all shared globals and `#include`s every `src/app/*.inc` file. The `.inc` extension is deliberate — do not convert these to `.cpp` files without also adding headers and `extern` declarations.

### Fragment responsibilities

| File | Owns |
|---|---|
| `src/main.cpp` | Globals, hardware constants, feature flags, `setup()`, `loop()` |
| `src/app/helpers.inc` | `DeviceConfig` struct, SD config parsing, battery, brightness, timezone, formatting utils |
| `src/app/display_touch.inc` | LVGL flush callback, XPT2046 touch mapping |
| `src/app/crypto_data.inc` | SD ticker loading, CoinGecko current price and 30-day history fetching |
| `src/app/weather_data.inc` | OpenWeather current + forecast parsing |
| `src/app/runtime_ui.inc` | Page switching, touch navigation, refresh scheduling, data worker integration |
| `src/app/ui_build.inc` | LVGL object construction and styling |
| `src/app/web.inc` | All web routes (setup AP, view, tickers, secrets, brightness, coingecko, info) |
| `src/app/pair_trading.inc` | Pair candidate scoring and rendering |
| `src/app/trading_signal.inc` | Crypto signal scoring and rendering |

### FreeRTOS data worker

Network calls that would stall the main loop (CoinGecko prices, CoinGecko 30-day history) are dispatched as `DataJob` structs onto `dataJobQueue`. A background task processes them and writes results into `staged*` globals protected by `dataWorkerMutex`. The main loop polls `cryptoPriceResultReady` / `cryptoHistoryResultReady` each iteration and copies staged data into live state. Any new network work must follow this pattern — do not make blocking HTTP calls from `loop()` directly.

### SPI buses

| Bus | Pins | Used by |
|---|---|---|
| Default SPI | MISO 12, MOSI 13, SCLK 14 | TFT display + XPT2046 touch |
| HSPI (`sdSpi`) | MISO 19, MOSI 23, SCLK 18, CS 5 | SD card |

The SD card uses a dedicated `SPIClass sdSpi(HSPI)` instance. Never put the SD card on the display SPI bus — this was a hard-won lesson documented in `docs/CC_sd_card_minimal_integration_notes.md`.

## Feature Flags

Defined near the top of `src/main.cpp`:

| Flag | Default | Effect |
|---|---|---|
| `ENABLE_BATTERY_MONITOR` | `1` | ADC battery voltage and percentage display |
| `ENABLE_TRADING_SIGNALS` | `1` | Adds Signals page to the navigation cycle |
| `ENABLE_PRESENCE_SLEEP` | *(planned)* | LD2410C presence-based display sleep |
| `TOUCH_DEBUG` | `0` | Prints raw touch and tap-decision details to Serial |

## Runtime Configuration

At boot the app reads two files from the SD card root:

- `/secrets.txt` — key=value config for Wi-Fi, OpenWeather API key, location, timezone, mDNS hostname, brightness, and CoinGecko timing. Template: `sdcard/secrets.example.txt`.
- `/crypto_tickers.txt` — one ticker per line (`BTC`, or `BTC|bitcoin|0`). Template: `sdcard/crypto_tickers.txt`.

If Wi-Fi credentials are absent or connection fails, the app starts a setup AP (`cloudandcoin-setup`, `http://192.168.4.1/secrets`).

Compile-time credential fallbacks live in `src/secrets.h` (git-ignored). Template: `src/secrets.example.h`.

**Do not read, print, commit, or summarize** `src/secrets.h` or `LongLivedAccessToken.txt`.

## Web Routes

When online at `http://cloudandcoin.local/`:

| Route | Purpose |
|---|---|
| `/` and `/view` | Dashboard web view |
| `/tickers` | Crypto ticker list editor |
| `/lookup` | CoinGecko coin lookup |
| `/brightness` | Backlight control |
| `/coingecko` | CoinGecko refresh timing editor |
| `/secrets` | Runtime config editor |
| `/info` | Device status (firmware, memory, battery) |

If `web_password` is set in `/secrets.txt`, all routes require HTTP Basic auth (`admin` / `<password>`).

## UI Page Cycle

Touch/tap advances pages:

```
Weather → Crypto → Pair Trading → [Signals] → Weather
```

Signals is removed from the cycle when `ENABLE_TRADING_SIGNALS` is `0`.

Crypto page layout switches automatically based on ticker count:
- **1–4 tickers**: fixed layout with 30-day sparklines
- **5–10 tickers**: scrolling list, sparklines disabled

## Pin Constraints

- **IO34**: Battery ADC (ADC1 — safe with Wi-Fi active).
- **IO35**: Reserved for future charging-status input (TP4054 CHRG). Do not reassign.
- **IO39**: Reserved for LD2410C presence sensor OUT (input-only GPIO, no accidental drive risk).
- **IO18, IO19, IO23**: SD card HSPI — do not use for other peripherals.
- **IO32, IO25**: I2C connector pins with 4.7 kΩ pull-ups to 3.3V. Planned for LD2410C UART in Phase 5.

## Planned Work: LD2410C Presence Sensor

Design and implementation details are in:
- `docs/CC_ld2410c_presence_sleep_plan.md` (ChatGPT — hardware research and phased plan)
- `docs/CC_ld2410c_presence_implementation.md` (Claude — state machine, GPIO, sleep spec)

Phase 1–4 covers OUT-pin-only presence detection with a four-state machine (ACTIVE → DIMMED → SLEEPING → WAKING) using `esp_light_sleep_start()`. Phase 5 adds UART distance filtering via IO32/IO25. New code lives in `src/app/presence_sensor.inc`.

## Key Constraints

- **JSON heap**: `DynamicJsonDocument` sizes matter — ESP32 RAM is tight. Filter or parse API responses narrowly rather than allocating large documents.
- **Loop responsiveness**: `loop()` must stay non-blocking. Long network work belongs in the data worker queue.
- **Touch after SD init**: Changing display or SPI setup can break touch/page navigation. Verify page navigation still works after any touch or SPI change.
- **Refresh timing**: Read `docs/CC_refresh_timing_reference.md` before adjusting weather or CoinGecko scheduling.

## Documentation Convention

Docs follow `CC_<subject>_<kind>.md` in `docs/`. Update relevant docs when changing setup flow, hardware, config keys, web routes, or API timing. Key reference docs:

- `docs/CC_quick_install_guide.md` — first-boot SD setup
- `docs/CC_refresh_timing_reference.md` — refresh scheduling logic
- `docs/CC_features_reference.md` — feature overview
- `docs/CC_freertos_reference.md` — plain-language FreeRTOS background
