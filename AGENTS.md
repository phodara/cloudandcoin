# Cloud And Coin Project Guide

## Project Overview

Cloud and Coin is PlatformIO/Arduino firmware for an ESP32 touchscreen
dashboard. It uses LVGL 8.3 and TFT_eSPI to drive a landscape 480 x 320 TFT,
reads XPT2046 touch input, stores runtime configuration on an SD card, fetches
weather from OpenWeather, fetches crypto prices/history from CoinGecko, and
serves local web pages for viewing and configuration.

The current app pages are:

- Weather
- Crypto
- Pair Trading
- Signals, when `ENABLE_TRADING_SIGNALS` is `1`

This repository is licensed under the PolyForm Noncommercial License 1.0.0.
Keep `LICENSE`, `NOTICE`, and Paul Hodara copyright notices intact.

## Secrets

Do not read, print, commit, summarize, or copy real credentials. In particular:

- `LongLivedAccessToken.txt` is intentionally ignored and should be treated as
  secret material.
- `src/secrets.h` contains local compile-time credentials and should remain
  ignored.
- Use `src/secrets.example.h` and `sdcard/secrets.example.txt` for templates.

Runtime secrets normally live on the SD card in `/secrets.txt`.

## Important Files

- `src/main.cpp`: shared globals, hardware constants, setup/loop orchestration,
  feature flags, and inclusion of app fragments.
- `src/app/*.inc`: implementation fragments included directly by `main.cpp`.
- `src/app/helpers.inc`: config parsing, SD helpers, battery, brightness,
  formatting, timezone, and small utilities.
- `src/app/web.inc`: setup AP pages, web view, ticker editor, secrets editor,
  brightness, CoinGecko timing, and info page routes.
- `src/app/display_touch.inc`: LVGL display flush and mapped touch input.
- `src/app/crypto_data.inc`: SD ticker loading, CoinGecko current prices, and
  30-day history fetching.
- `src/app/weather_data.inc`: OpenWeather current weather and forecast parsing.
- `src/app/runtime_ui.inc`: page switching, touch navigation, refresh
  scheduling, FreeRTOS data worker integration, and drawing updates.
- `src/app/pair_trading.inc`: pair candidate scoring/rendering.
- `src/app/trading_signal.inc`: informational crypto signal scoring/rendering.
- `src/app/ui_build.inc`: LVGL object construction and styling.
- `include/lv_conf.h`: LVGL configuration.
- `platformio.ini`: board target, library dependencies, display/touch defines.
- `sdcard/`: sample SD-card files.

The `.inc` layout is deliberate: the firmware builds as one C++ translation
unit while keeping responsibilities separated. If converting to `.cpp` files,
add headers and explicit `extern` declarations instead of relying on include
order.

## Build And Upload

PlatformIO environment:

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

Current `platformio.ini` pins upload and monitor to:

```text
/dev/cu.usbserial-10
```

If the ESP32 appears on a different port, update or override those settings.
Upload speed is `460800`; monitor speed is `115200`.

## Hardware

Target board used by this project:

```text
Hosyond 3.5'' 320x480 Touch Screen ESP32 Display with WiFi+BT,
ST7796U Driver LCD TFT Screen Module
```

The firmware uses landscape `480 x 320` UI coordinates.

Display/touch flags are in `platformio.ini`, so do not edit TFT_eSPI library
files for this project.

Key pins:

- TFT MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST -1, BL 27.
- Touch CS 33.
- SD card CS 5, SCK 18, MISO 19, MOSI 23.
- Battery ADC 34.

Important SPI rule: the SD card uses a separate `SPIClass sdSpi(HSPI)` and must
not reuse the display/touch SPI path. Earlier SD work showed that using the
wrong SPI host can break touch/page navigation.

## Runtime Configuration

The app tries to read these SD-root files:

```text
/secrets.txt
/crypto_tickers.txt
```

`/secrets.txt` is key/value config for Wi-Fi, web password, OpenWeather,
location, timezone, mDNS hostname, screen brightness, and CoinGecko timing.

`/crypto_tickers.txt` supports comments and ticker lines such as:

```text
BTC
BTC|bitcoin|0
ADA|cardano|3
```

The full current setup flow is in `docs/CC_quick_install_guide.md`.

If SD config is missing, the app falls back to compiled defaults from
`src/secrets.h`/`src/secrets.example.h`. If Wi-Fi SSID is missing or connection
fails, it starts a temporary setup AP:

```text
SSID: cloudandcoin-setup
URL:  http://192.168.4.1/secrets
```

When online, typical routes are:

- `http://cloudandcoin.local/` and `/view`
- `/tickers`
- `/lookup`
- `/brightness`
- `/coingecko`
- `/secrets`
- `/info`

If `web_password` is set, web pages use HTTP Basic auth with username `admin`.

## Data And Timing

Weather:

- Current weather refreshes every 15 seconds when Wi-Fi is connected and the
  data worker is not busy.
- Forecast refreshes every 3 hours or when missing.

CoinGecko:

- Current prices are fetched in batches of up to 4 ids.
- 30-day history is fetched one configured coin at a time.
- Current-price requests pause while a history refresh cycle is active.
- HTTP 429 rate limits trigger retry/backoff timing from `/secrets.txt`.

See `docs/CC_refresh_timing_reference.md` before changing refresh scheduling.

Network calls are queued through a FreeRTOS data worker so the main loop can
keep LVGL, touch, web handling, battery updates, and drawing responsive.

## UI Behavior

Touch navigation advances pages on press/tap:

```text
Weather -> Crypto -> Pair Trading -> Signals -> Weather
```

When `ENABLE_TRADING_SIGNALS` is `0`, the Signals page is removed from the
cycle.

Crypto layout depends on ticker count:

- 1-4 configured tickers: fixed crypto page with sparklines.
- 5-10 configured tickers: scrolling list, sparklines disabled.

Pair Trading and Signals are informational only. They do not place trades,
connect to exchanges, manage risk, or guarantee price movement.

## Power And Battery

Battery display is enabled by `ENABLE_BATTERY_MONITOR` and reads ADC pin 34
through a configured divider. The app shows approximate voltage/percentage in
the device UI and web view.

Hardware modification notes live in:

- `docs/CC_charging_monitor_wiring_notes.md`
- `docs/CC_power_switch_notes.md`

Treat those as hardware notes, not implemented firmware behavior unless the
corresponding code and wiring exist.

## Documentation Naming

Docs in `docs/` use this pattern:

```text
CC_<subject>_<kind>.<ext>
```

Use `CC_` as the project prefix, lowercase underscore-separated subject words,
and a final kind such as `guide`, `reference`, `plan`, `notes`, or `spec`.
Screenshots under `docs/images/` also use the `CC_` prefix.

## Documentation Map

- `docs/CC_quick_install_guide.md`: SD-card setup and first boot.
- `docs/CC_features_reference.md`: feature overview and web routes.
- `docs/CC_refresh_timing_reference.md`: OpenWeather/CoinGecko refresh logic.
- `docs/CC_crypto_trade_signals_reference.md`: current Signals and Pair Trading
  calculations and interpretation.
- `docs/CC_crypto_scaling_plan.md`: future larger ticker-list design.
- `docs/CC_lightweight_crypto_analysis_plan.md`: future lightweight analysis
  ideas.
- `docs/CC_pair_trading_plan.md`: older/future pair-trading design notes.
- `docs/CC_sd_card_minimal_integration_notes.md`: historical SD integration
  notes and HSPI constraint.
- `docs/CC_charging_monitor_wiring_notes.md`: hardware charging-status wiring
  idea.
- `docs/CC_power_switch_notes.md`: hardware power-switch notes.
- `docs/CC_freertos_reference.md`: plain-language FreeRTOS overview.

## Coding Guidance

- Do not commit secrets or local credential files.
- Keep display/touch pin defines and TFT_eSPI setup flags in `platformio.ini`
  unless there is a deliberate hardware migration.
- Keep SD on `HSPI` and avoid changing the display/touch SPI setup casually.
- Preserve responsive loop behavior: long network work belongs in the data
  worker or behind existing queue/backoff logic.
- Update `README.md` and relevant `docs/CC_*` files when setup, hardware,
  config format, web routes, or API timing changes.
- Be careful with `DynamicJsonDocument` sizes; ESP32 RAM/flash are tight enough
  that larger API responses should be filtered or parsed narrowly.
- When touching crypto behavior, test both 1-4 ticker sparklines and 5-10 ticker
  scrolling-list mode.
- When changing touch/display behavior, confirm page navigation still works
  after SD card initialization.

