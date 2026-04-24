# Features

`Cloud & Coin` is an ESP32 touchscreen dashboard for weather and cryptocurrency monitoring.

## Core Features

- Landscape touchscreen interface built with LVGL and TFT_eSPI
- Live current weather display
- 4-day OpenWeather forecast
- Current crypto prices from CoinGecko
- Local web editor for settings and ticker management
- SD-card-based runtime configuration
- Wi-Fi setup fallback through a temporary setup network

## Weather Screen

- Current temperature
- Current condition summary
- Daily high and low
- Pressure reading
- 4-day forecast cards

## Crypto Screen

- Configurable crypto ticker list from `/crypto_tickers.txt`
- Supports comments with `#`
- Color-coded price movement
- Neutral `-` marker until a comparison baseline exists
- Keeps the last good value on screen if a refresh fails

## Crypto Display Behavior

- `1-4` configured tickers: fixed crypto layout with sparklines
- `5-10` configured tickers: scrolling crypto list without sparklines

## Web Interface

- `http://weathercrypto.local/tickers`
- `http://weathercrypto.local/secrets`
- `http://weathercrypto.local/info`

The web pages use the `Cloud and Coin` branding and support immediate ticker reload after saving.
