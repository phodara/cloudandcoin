# Features

`Cloud and Coin` is an ESP32 touchscreen dashboard for weather and cryptocurrency monitoring.

## Core Features

- Landscape touchscreen interface built with LVGL and TFT_eSPI
- Live current weather display
- 4-day OpenWeather forecast
- Current crypto prices from CoinGecko
- Local web editor for settings and ticker management
- Web-controlled screen brightness saved on the SD card
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

Future pair-trading screen notes are captured in [`future-pair-trading.md`](future-pair-trading.md).

Future lightweight crypto analysis ideas are captured in [`future-lightweight-crypto-analysis.md`](future-lightweight-crypto-analysis.md).

Weather and CoinGecko refresh behavior is documented in [`refresh-timing.md`](refresh-timing.md).

## Web Interface

- `http://cloudandcoin.local/`
- `http://cloudandcoin.local/view`
- `http://cloudandcoin.local/tickers`
- `http://cloudandcoin.local/brightness`
- `http://cloudandcoin.local/coingecko`
- `http://cloudandcoin.local/secrets`
- `http://cloudandcoin.local/info`

The web pages use the `Cloud and Coin` branding, show battery/memory/network/local-time status, and support immediate ticker reload after saving.

## Debug Flags

- `TOUCH_DEBUG`: set to `1` to print raw touch and tap decision details to Serial; defaults to `0`.
