<<<<<<< HEAD
# cloudandcoin
=======
# CloudAndCoin

ESP32 touchscreen dashboard built with PlatformIO, LVGL, and TFT_eSPI. The display pulls live weather and crypto data from Home Assistant, fetches a 4-day forecast from OpenWeather, and renders 30-day sparkline history for supported coins.

For the current SD-card setup flow, see [`docs/quick-install.md`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/docs/quick-install.md:1).

## What It Does
- Shows the current weather, condition, high, low, and pressure
- Displays a 4-day OpenWeather forecast
- Tracks BTC, ETH, and ADA prices from Home Assistant
- Draws 30-day sparkline history using CoinGecko data
- Supports swipe navigation between weather and crypto screens
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

Display-related settings are defined in [`platformio.ini`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/platformio.ini:1), so you do not need to edit `TFT_eSPI` library files manually.

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
- Home Assistant entities for current weather and live crypto prices
- OpenWeather for the 4-day forecast
- CoinGecko for 30-day BTC, ETH, and ADA price history

## Required Secrets
Copy [`src/secrets.example.h`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/src/secrets.example.h:1) to `src/secrets.h` and fill in:

- `SECRET_SSID`
- `SECRET_WIFI_PASS`
- `SECRET_HA_TOKEN`
- `SECRET_OWM_API`

`src/secrets.h` is intentionally ignored by git so credentials do not get pushed to GitHub.

## Home Assistant Entities
The current code expects these entity IDs in [`src/main.cpp`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/src/main.cpp:27):

- `weather.openweathermap`
- `sensor.bitcoin_usd`
- `sensor.ethereum_usd`
- `sensor.cardano_usd`
- `sensor.openweathermap_pressure`

If your Home Assistant setup uses different entity names, update the constants near the top of `src/main.cpp`.

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

Example file:
- [`sdcard/crypto_tickers.txt`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/sdcard/crypto_tickers.txt:1)

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
- The ticker list reloads at boot and after saving through the web editor

If the SD card is missing, the file is missing, or no supported tickers are found, the app falls back to its default list.

## Quick Start
1. Install VS Code and the PlatformIO extension.
2. Open this project folder in VS Code.
3. Copy `src/secrets.example.h` to `src/secrets.h` and add your real credentials.
4. Review the Home Assistant entity IDs in `src/main.cpp`.
5. Connect the ESP32.
6. Build the project.
7. Upload the firmware.
8. Open the serial monitor at `115200` if you want runtime logs.

## Build Notes
- Board target: `esp32dev`
- Framework: Arduino
- C++ standard: `gnu++17`
- Upload speed: `115200`
- Monitor speed: `115200`

## Repository Layout
- [`src/main.cpp`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/src/main.cpp:1): main application logic and UI
- [`src/secrets.example.h`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/src/secrets.example.h:1): safe template for local credentials
- [`include/lv_conf.h`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/include/lv_conf.h:1): LVGL configuration
- [`platformio.ini`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/platformio.ini:1): board, libraries, and display flags

## Notes
- The display is configured for landscape orientation with a 480x320 framebuffer.
- Forecast refresh, history refresh, and UI behavior are defined in `src/main.cpp`.
- Touch calibration values are currently hardcoded for this hardware setup.

## License
This project is licensed under [`PolyForm Noncommercial 1.0.0`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/LICENSE:1).
Commercial use is not permitted without separate permission from the licensor.
Redistributors must keep the license terms or license URL and the included `Required Notice`.
See [`NOTICE`](/Users/paulhodara/Documents/PlatformIO/cloudandcoin/NOTICE:1) for the repository notice that should stay with redistributed copies.
>>>>>>> 4a4265d (Update project files)
