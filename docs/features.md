# Features

`Cloud and Coin` is an ESP32 touchscreen dashboard for weather and cryptocurrency monitoring.

## Core Features

- Landscape touchscreen interface built with LVGL and TFT_eSPI
- Live current weather display
- 4-day OpenWeather forecast
- Current crypto prices from CoinGecko
- Local web editor for settings and ticker management
- SD-card-based runtime configuration
- Temporary remote configuration network for first-time Wi-Fi setup and recovery

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
- 30-day history is loaded in the background for sparkline-capable layouts and missing history is retried

Future larger-list design notes are captured in [`future-crypto-scaling.md`](future-crypto-scaling.md).

## Web Interface

- `http://cloudandcoin.local/`
- `http://cloudandcoin.local/view`
- `http://cloudandcoin.local/tickers`
- `http://cloudandcoin.local/secrets`
- `http://cloudandcoin.local/info`

The web pages use the `Cloud and Coin` branding, show battery/memory/network/local-time status, and support immediate ticker reload after saving.
