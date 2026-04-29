# Cloud and Coin

ESP32 touchscreen dashboard built with PlatformIO, LVGL, and TFT_eSPI. The display pulls current weather and a 4-day forecast from OpenWeather, fetches live crypto prices and history from CoinGecko, and can be viewed or configured over Wi-Fi.

For the current SD-card setup flow, see [`docs/quick-install.md`](docs/quick-install.md).

## What It Does
- Shows the current weather, condition, high, low, and pressure
- Displays a 4-day OpenWeather forecast
- Tracks configurable cryptocurrency prices from CoinGecko
- Draws 30-day sparkline history using CoinGecko data
- Supports swipe navigation between weather and crypto screens
- Provides a responsive web view for remote weather, crypto, battery, memory, and local time status
- Lets you adjust screen brightness from the web UI and persist it on the SD card
- Runs on an ESP32 with an ILI9486 TFT and XPT2046 touch controller

## Stack
- PlatformIO
- Arduino framework for ESP32
- LVGL 8.3
- TFT_eSPI
- XPT2046_Touchscreen
- ArduinoJson

## Hardware Assumptions
- ESP32 dev board
- 480x320 ILI9486 TFT display
- XPT2046 touch controller

This project is currently built and tested on:

- Hosyond `3.5'' 320x480` Touch Screen ESP32 Display with WiFi+BT, ST7796U Driver LCD TFT Screen Module
- Product link: <https://a.co/d/0cVFruZA>

Display-related settings are defined in [`platformio.ini`](platformio.ini), so you do not need to edit `TFT_eSPI` library files manually.

## Pin Configuration
- `TFT_MISO`: 12
- `TFT_MOSI`: 13
- `TFT_SCLK`: 14
- `TFT_CS`: 15
- `TFT_DC`: 2
- `TFT_RST`: -1
- `TFT_BL`: 27
- `TOUCH_CS`: 33

## Data Sources
- OpenWeather for current weather and the 4-day forecast
- CoinGecko for current crypto prices and 30-day price history

## Required Secrets
Copy [`src/secrets.example.h`](src/secrets.example.h) to `src/secrets.h` and fill in:

- `SECRET_SSID`
- `SECRET_WIFI_PASS`
- `SECRET_OWM_API`

`src/secrets.h` is intentionally ignored by git so credentials do not get pushed to GitHub.

## SD Card Crypto List
At boot, the app tries to read `/crypto_tickers.txt` from the root of the SD card.

Format:
- One ticker per line
- Blank lines are ignored
- Lines starting with `#` are ignored
- Use `|` as the separator
- Supported line formats:
- `BTC`
- `BTC|bitcoin|0`
- `ADA|cardano|3`

Format details:
- field 1: display symbol
- field 2: CoinGecko coin id
- field 3: number of decimal places to display

Currently supported tickers:
- `BTC`
- `ETH`
- `ADA`
- `DOGE`

Example file:
- [`sdcard/crypto_tickers.txt`](sdcard/crypto_tickers.txt)

Example entries:

```txt
BTC|bitcoin|0
ETH|ethereum|0
ADA|cardano|3
# SOL|solana|2
```

Display behavior:
- `1-4` configured tickers: fixed crypto page with sparklines enabled
- `5-10` configured tickers: scrolling crypto list, sparklines disabled
- Crypto prices are fetched once at boot, every 60 seconds on the Crypto screen, and every 5 minutes while the device stays on Weather
- The ticker list reloads at boot and after saving through the web editor

If the SD card is missing, the file is missing, or no supported tickers are found, the app falls back to its default list.

## Quick Start
1. Install VS Code and the PlatformIO extension.
2. Open this project folder in VS Code.
3. Copy `src/secrets.example.h` to `src/secrets.h` and add your real credentials.
4. Put `secrets.txt` and `crypto_tickers.txt` on the SD card, or use the temporary setup network on first boot.
5. Connect the ESP32.
6. Build the project.
7. Upload the firmware.
8. Open the serial monitor at `115200` if you want runtime logs.

## Build Notes
- Board target: `esp32dev`
- Framework: Arduino
- C++ standard: `gnu++17`
- Upload speed: `460800`
- Monitor speed: `115200`

## Repository Layout
- [`src/main.cpp`](src/main.cpp): shared firmware state plus setup and loop orchestration
- [`src/app/`](src/app/): focused app source fragments included by `main.cpp`
- [`src/secrets.example.h`](src/secrets.example.h): safe template for local credentials
- [`include/lv_conf.h`](include/lv_conf.h): LVGL configuration
- [`platformio.ini`](platformio.ini): board, libraries, and display flags

## Notes
- The display is configured for landscape orientation with a 480x320 framebuffer.
- Forecast refresh, history refresh, and UI behavior are split across `src/main.cpp` and `src/app/`.
- Touch calibration values are currently hardcoded for this hardware setup.

## License
This project is licensed under [`PolyForm Noncommercial 1.0.0`](LICENSE).
Commercial use is not permitted without separate permission from the licensor.
Redistributors must keep the license terms or license URL and the included `Required Notice`.
See [`NOTICE`](NOTICE) for the repository notice that should stay with redistributed copies.
