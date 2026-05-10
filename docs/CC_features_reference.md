# Features

`Cloud and Coin` is an ESP32 touchscreen dashboard for weather, cryptocurrency, and stock monitoring.

## Core Features

- Landscape touchscreen interface built with LVGL and TFT_eSPI
- Live current weather display
- 4-day OpenWeather forecast
- Current crypto prices from CoinGecko
- Current stock quotes from Finnhub
- Local web editor for settings and ticker management
- Web-controlled screen brightness saved on the SD card
- SD-card-based runtime configuration
- Up to 10 saved Wi-Fi networks with boot-time network scanning
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

## Stocks Screen

- Configurable stock ticker list from `/stock_tickers.txt`
- Uses `finnhub_api_key` from `/secrets.txt`
- Shows current quote price and day percent change
- Supports comments with `#`
- `1-4` configured tickers: fixed stock layout
- `5-10` configured tickers: scrolling stock list
- Keeps the last good value on screen if a refresh fails

Current crypto trade signals and pair analysis are documented in [`CC_crypto_trade_signals_reference.md`](CC_crypto_trade_signals_reference.md).

Future larger-list design notes are captured in [`CC_crypto_scaling_plan.md`](CC_crypto_scaling_plan.md).

Future pair-trading screen notes are captured in [`CC_pair_trading_plan.md`](CC_pair_trading_plan.md).

Future lightweight crypto analysis ideas are captured in [`CC_lightweight_crypto_analysis_plan.md`](CC_lightweight_crypto_analysis_plan.md).

Weather, CoinGecko, and Finnhub refresh behavior is documented in [`CC_refresh_timing_reference.md`](CC_refresh_timing_reference.md).

## Web Interface

- `http://cloudandcoin.local/`
- `http://cloudandcoin.local/view`
- `http://cloudandcoin.local/tickers`
- `http://cloudandcoin.local/stocks`
- `http://cloudandcoin.local/lookup`
- `http://cloudandcoin.local/brightness`
- `http://cloudandcoin.local/coingecko`
- `http://cloudandcoin.local/secrets`
- `http://cloudandcoin.local/info`

The web pages use the `Cloud and Coin` branding, show battery/memory/network/local-time status, and support immediate crypto and stock ticker reload after saving.

## Debug Flags

- `TOUCH_DEBUG`: set to `1` to print raw touch and tap decision details to Serial; defaults to `0`.
