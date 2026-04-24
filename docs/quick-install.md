# Quick Install

This guide covers the fastest way to get `WeatherCrypto` running with the new SD-card-based setup flow.

## What You Need

- An ESP32 flashed with the current firmware
- The board used for this project: Hosyond `3.5'' 320x480` Touch Screen ESP32 Display with WiFi+BT, ST7796U Driver LCD TFT Screen Module
- Product link: <https://a.co/d/0cVFruZA>
- A microSD card
- Your Wi-Fi network name and password
- Your OpenWeather API key

## SD Card Files

Place these files in the root of the SD card:

- `secrets.txt`
- `crypto_tickers.txt`

You can start from:

- [`sdcard/secrets.example.txt`](../sdcard/secrets.example.txt)
- [`sdcard/crypto_tickers.txt`](../sdcard/crypto_tickers.txt)

## secrets.txt Format

Create `/secrets.txt` on the SD card with plain `key=value` lines and no quotes:

```txt
wifi_ssid=YOUR_WIFI_SSID
wifi_password=YOUR_WIFI_PASSWORD
web_password=CHANGE_ME
owm_api_key=YOUR_OPENWEATHER_API_KEY
weather_location=Mount Kisco,US
timezone=America/New_York
mdns_hostname=weathercrypto
```

Notes:

- Do not wrap values in quotes.
- One setting per line.
- Blank lines are fine.
- Lines starting with `#` are treated as comments.
- `owm_api_key` must be your active OpenWeather API key.
- `weather_location` should use a city and country code such as `Mount Kisco,US`.

## crypto_tickers.txt Format

Create `/crypto_tickers.txt` in the root of the SD card.

Rules:

- One crypto per line
- Use `|` as the separator
- Blank lines are ignored
- Lines starting with `#` are ignored

Supported line formats:

```txt
BTC
BTC|bitcoin|0
ADA|cardano|3
```

Field meanings for the `SYMBOL|coingecko-id|decimals` format:

- field 1: display symbol
- field 2: CoinGecko id used for price/history lookup
- field 3: number of decimal places shown on screen

Example file:

```txt
BTC|bitcoin|0
ETH|ethereum|0
ADA|cardano|3
DOGE|dogecoin|3
# SOL|solana|2
```

Display behavior:

- `1-4` configured tickers: fixed crypto page with sparklines enabled
- `5-10` configured tickers: scrolling crypto list, sparklines disabled
- Saving from `http://weathercrypto.local/tickers` reloads the ticker file immediately

## First Boot Options

You have two setup paths.

### Option 1: Preload secrets.txt

1. Put a completed `secrets.txt` on the SD card.
2. Insert the SD card and power on the device.
3. The device will read the file and try to join your Wi-Fi network.
4. If successful, browse to:
   - `http://weathercrypto.local/tickers`
   - `http://weathercrypto.local/secrets`
   - `http://weathercrypto.local/info`

### Option 2: Use the Temporary Setup Network

If `wifi_ssid` is missing or Wi-Fi connection fails, the device starts a temporary setup network automatically.

On the display you will see:

```text
Initial Setup

1. Join the temporary network:
   weathercrypto-setup

2. Open:
   192.168.4.1/setup

3. Add your local network settings.
```

Then:

1. Connect your phone or computer to `weathercrypto-setup`
2. Open `http://192.168.4.1/setup`
3. Enter your local Wi-Fi, web password, and any other settings
4. Save and reboot

## Web Password

The web pages use HTTP Basic auth when `web_password` is set.

- Username: `admin`
- Password: the value of `web_password` in `secrets.txt`

## Editing After Install

After the device is online:

- `http://weathercrypto.local/tickers` edits `/crypto_tickers.txt`
- `http://weathercrypto.local/secrets` edits `/secrets.txt`
- `http://weathercrypto.local/info` shows repository, license, notice, and contribution details

## Notes About Saving

- `Save Secrets` applies weather-related changes immediately
- `Save And Reboot` saves the edited secrets file and reboots the device
- Wi-Fi and hostname changes are best applied with reboot

## Troubleshooting

If the serial monitor shows:

```text
Secrets: secrets.txt missing, using compiled defaults
```

then the device did not find `/secrets.txt` in the root of the SD card.

Check:

- the file name is exactly `secrets.txt`
- the file is in the root of the SD card
- the card is inserted correctly

If the log shows:

```text
OWM current: HTTP 401
```

then the OpenWeather API key in `secrets.txt` is invalid, inactive, or entered incorrectly.
