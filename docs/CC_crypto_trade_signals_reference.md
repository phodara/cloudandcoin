# Crypto Trade Signals and Analysis

Cloud and Coin includes lightweight crypto analysis screens that reuse the price data the device already fetches from CoinGecko. The goal is to make the dashboard more useful at a glance without turning the ESP32 into a trading bot.

These signals are informational only. They do not place trades, connect to an exchange, manage risk, or guarantee future price movement.

## Screens

The crypto analysis features appear in two places:

- Device touchscreen pages
- Local web view at `http://cloudandcoin.local/view`

The current page flow is:

```text
Weather -> Crypto -> Pair Trading -> Signals -> Weather
```

The `Signals` page is controlled by this build flag in `src/main.cpp`:

```cpp
#define ENABLE_TRADING_SIGNALS 1
```

Set it to `0` to remove the Signals page from the firmware build.

## Data Used

The analysis uses the same data as the crypto display:

- Configured symbols and CoinGecko ids from `/crypto_tickers.txt`
- Current USD prices from CoinGecko
- 30-day daily price history from CoinGecko

No extra exchange connection, account data, order book data, or private API keys are used. The analysis waits for history data before showing complete results.

## Trading Signals

The Signals page gives each configured crypto a compact scorecard. Each row includes:

```text
SYMBOL PRICE ACTION SCORE REASON
```

Example:

```text
BTC $77,057 BUY +4 Near 30d high
```

The score is a simple composite of recent trend and momentum checks. Bullish conditions add points. Bearish conditions subtract points.

| Check | Bullish | Bearish |
| --- | --- | --- |
| Current price vs 7-day average | `+1` if above | `-1` if below |
| 14-day average vs 30-day average | `+1` if 14-day is above 30-day | `-1` if 14-day is below 30-day |
| 30-day return | `+1` if greater than `+5%` | `-1` if less than `-5%` |
| Latest price move | `+1` if greater than `+2%` | `-1` if less than `-2%` |
| Position in 30-day range | `+1` near the 30-day high | `-1` near the 30-day low |

The final score maps to short labels:

| Score | Label | Meaning |
| ---: | --- | --- |
| `+3` or higher | `BUY` | Stronger bullish alignment across the checks |
| `+1` to `+2` | `WATCH` | Mild bullish momentum |
| `0` | `HOLD` | Mixed or neutral conditions |
| `-1` to `-2` | `WATCH` | Mild bearish momentum |
| `-3` or lower | `SELL` | Stronger bearish alignment across the checks |
| Missing data | `WAIT` | Current price or history is not ready |

`WATCH` can appear for both positive and negative scores. Read the score and reason text to understand the direction.

## Pair Trading Analysis

The Pair Trading page ranks combinations of the configured cryptos. It looks for pairs that recently moved together but now show a meaningful ratio divergence.

Each row includes:

```text
PAIR SIGNAL Z-SCORE CORRELATION BIAS
```

Example:

```text
BTC/ETH Strong z-2.1 c0.86 Long BTC
```

The calculation uses:

- Daily log returns over the 30-day history window
- Return correlation between each pair
- The historical log price ratio between the two assets
- A z-score for the latest ratio compared with its recent mean and standard deviation

Pairs are ranked by:

```text
absolute z-score * positive correlation
```

That means highly correlated pairs with larger divergences rise to the top.

## Pair Signal Strength

Pair Trading labels use correlation and z-score thresholds:

| Label | Rule |
| --- | --- |
| `Strong` | Correlation is at least `0.70` and absolute z-score is at least `2.0` |
| `Med` | Correlation is at least `0.55` and absolute z-score is at least `1.3` |
| `Weak` | Anything below the medium threshold |

The bias text is based on which side of the ratio looks lower than usual:

- `Long SYMBOL_A` when the pair ratio is below its recent average
- `Long SYMBOL_B` when the pair ratio is above its recent average
- `Watch` when the z-score is less than `1.0` in either direction

This is a directional bias, not a complete trade plan. A real pair trade would also need position sizing, short availability, fees, liquidity checks, stop rules, and risk limits.

## Refresh Behavior

Current prices and history are refreshed by the existing CoinGecko scheduler. Pair Trading and Signals recalculate from the latest in-memory values when their pages render.

Useful details:

- Current prices refresh more often while viewing Crypto, Pair Trading, or Signals.
- History loads one configured coin at a time.
- Pair Trading and Signals may show waiting states until history is available.
- If CoinGecko rate-limits requests, retry timing follows the settings documented in `CC_refresh_timing_reference.md`.

## Display Limits

The device screen intentionally shows compact summaries:

- Pair Trading shows up to four ranked candidates.
- Signals shows up to four configured cryptos.
- Longer configured ticker lists are easier to inspect from the web view.

This keeps the ESP32 UI readable and avoids crowding the 480x320 display.

## Practical Interpretation

Use the signals as a quick status layer:

- `BUY` or positive `WATCH`: momentum and trend checks are leaning bullish.
- `SELL` or negative `WATCH`: momentum and trend checks are leaning bearish.
- `HOLD`: conditions are mixed.
- `WAIT`: data is still loading or unavailable.
- `Strong` pair: the pair is historically related and currently stretched.
- `Weak` pair: the relationship or divergence is not strong enough to emphasize.

Crypto markets can change faster than the 30-day daily history window captures. Token news, liquidity shocks, unlocks, exchange outages, regulatory events, and broad market moves can invalidate a signal quickly.

## Implementation Files

The main implementation lives in:

- `src/app/trading_signal.inc`
- `src/app/pair_trading.inc`
- `src/app/ui_build.inc`
- `src/app/web.inc`
- `src/app/crypto_data.inc`

Related documentation:

- `CC_refresh_timing_reference.md`
- `CC_lightweight_crypto_analysis_plan.md`
- `CC_pair_trading_plan.md`
