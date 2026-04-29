# Weather and Crypto Refresh Timing

This document explains when the firmware calls OpenWeather and CoinGecko, which settings control those calls, and how rate-limit backoff works.

## Weather Timing

Weather timing is currently fixed in firmware:

| Data | Default Timing | Notes |
| --- | ---: | --- |
| Current weather | 15 seconds | Runs when Wi-Fi is connected and the data worker is not busy. |
| 4-day forecast | 3 hours | Checked during weather refresh; fetched when the forecast interval has elapsed or forecast data is missing. |

Current weather refreshes are skipped if Wi-Fi is disconnected. Forecast data is requested less often because it changes slowly and is a heavier API call than updating the current conditions.

## CoinGecko Timing Settings

CoinGecko timing is configurable from:

```text
http://cloudandcoin.local/coingecko
```

The same values are stored in `/secrets.txt`, so they survive reboot.

Default values:

```txt
cg_current_refresh_seconds=60
cg_current_retry_minutes=5
cg_background_refresh_minutes=5
cg_web_refresh_seconds=60
cg_history_step_seconds=15
cg_history_retry_minutes=5
cg_history_refresh_hours=2
```

## CoinGecko Setting Meanings

| Setting | Default | Unit | Meaning |
| --- | ---: | --- | --- |
| `cg_current_refresh_seconds` | 60 | seconds | How often current prices refresh while viewing Crypto, Pair Trading, or Signals. |
| `cg_current_retry_minutes` | 5 | minutes | How long to wait before retrying current prices after CoinGecko returns HTTP 429. |
| `cg_background_refresh_minutes` | 5 | minutes | How often current prices refresh while the device is on Weather. |
| `cg_web_refresh_seconds` | 60 | seconds | Minimum time between web-view-triggered current-price refreshes. |
| `cg_history_step_seconds` | 15 | seconds | Delay between individual per-coin history requests. |
| `cg_history_retry_minutes` | 5 | minutes | How long to wait before retrying missing history after a failed history call or HTTP 429. |
| `cg_history_refresh_hours` | 2 | hours | How long to wait after all history is loaded before starting another full history cycle. |

## Current Price Flow

Current prices are fetched in batches. With four configured coins, one request asks CoinGecko for all four current prices.

Example serial output:

```text
CG current batch: bitcoin = 77057.000000
CG current batch: cardano = 0.248672
CG current batch: ethereum = 2310.409912
CG current batch: stellar = 0.161766
```

If the batch is rate-limited, the whole current-price batch failed:

```text
CG current batch: ids=bitcoin,cardano,ethereum,stellar HTTP 429
CG current batch: rate limited, retry after 5 minute(s)
```

When that happens, new current-price requests are paused for `cg_current_retry_minutes`.

## History Flow

History is fetched per coin, one coin at a time. The firmware waits `cg_history_step_seconds` between individual history calls.

Example serial output:

```text
CG history: bitcoin returned 31 points
CG history: cardano returned 31 points
CG history: ethereum returned 31 points
CG history: stellar returned 31 points
```

The API may return 31 daily points for a 30-day request. The firmware samples those values into the 30 points used by the display.

History can load while the device is on Weather, Crypto, Pair Trading, or Signals. The visible sparkline, pair, and signal pages are updated when those pages are shown.

## Current vs History

These are separate CoinGecko calls:

```text
CG current batch: stellar = 0.161769
```

means a current USD price was returned.

```text
CG history: stellar returned 31 points
```

means 30-day daily history data was returned.

## Scheduling Rules

- Current-price calls are skipped while a history refresh cycle is active.
- When a full history cycle completes, the current-price timers are reset so the firmware does not immediately call CoinGecko again.
- If current prices get HTTP 429, current-price retries wait `cg_current_retry_minutes`.
- If a history request fails or gets HTTP 429, only missing history is retried after `cg_history_retry_minutes`.
- After all configured coins have successful history, the firmware waits `cg_history_refresh_hours` before starting another full history cycle.
- Web-view-triggered current-price refreshes share timing with normal current-price refreshes so opening `/view` does not immediately duplicate a recent current-price request.

## Practical Guidance

If you see current-price HTTP 429 errors, increase:

```txt
cg_current_refresh_seconds
cg_background_refresh_minutes
cg_current_retry_minutes
```

If you see history HTTP 429 errors, increase:

```txt
cg_history_step_seconds
cg_history_retry_minutes
cg_history_refresh_hours
```

For a small ticker list, the defaults are intended to be responsive without being too aggressive. For larger ticker lists, increase the history spacing first.
