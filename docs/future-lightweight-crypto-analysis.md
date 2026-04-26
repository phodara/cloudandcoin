# Future Lightweight Crypto Analysis

This note captures low-resource crypto analysis ideas that could make Cloud and Coin more useful without turning the ESP32 into a heavy analytics engine.

## Goal

Add simple, explainable signals that reuse data the firmware already has:

- current crypto prices
- 30-day daily history
- configured ticker metadata from `/crypto_tickers.txt`
- existing pair-trading calculations

The best additions should be cheap to compute, easy to display, and clear that they are signals or context, not trade instructions.

## Recommended First Additions

### 1. Volatility Score

Show whether each configured crypto has been moving quietly or aggressively over the recent history window.

Possible labels:

- Low Vol
- Med Vol
- High Vol

Simple calculation:

- Use daily log returns from the existing 30-day history.
- Calculate standard deviation of those returns.
- Bucket the result into low, medium, or high volatility.

Why it is valuable:

- Helps explain whether price moves are normal noise or unusually jumpy.
- Adds useful risk context to both the crypto screen and pair-trading screen.
- Very cheap to compute from data already in memory.

Recommended display:

- Device: small text badge or abbreviated label such as `Vol H`.
- Web: add a volatility column or detail row for each crypto.

### 2. Outlier Alert

Flag assets whose current price is unusually far from their own recent average.

Possible labels:

- Normal
- Extended
- Deep Dip

Simple calculation:

- Compare current price against the 30-day average.
- Calculate a z-score using the 30-day price standard deviation.
- Treat large positive z-scores as extended and large negative z-scores as dips.

Why it is valuable:

- Easy to understand.
- Useful even with only one configured crypto.
- Reuses the same style of reasoning as pair trading, but for a single asset.

Recommended display:

- Device: show only when the signal is meaningful, such as `Dip` or `Ext`.
- Web: show z-score and plain-language label.

## Other Good Candidates

### 24h / 7d Trend Badge

Compare the latest price with recent historical points and show whether the asset is up or down over short and medium windows.

Resource cost:

- Very low.

Value:

- Helpful at a glance.
- Less statistically rich than volatility or outlier detection, but easy to understand.

### Momentum vs Mean-Reversion Label

Classify whether an asset appears to be trending or stretched.

Possible labels:

- Momentum
- Extended
- Reverting
- Flat

Resource cost:

- Low to medium.

Value:

- More expressive than a simple up/down arrow.
- Needs careful wording to avoid pretending the device can predict future movement.

### Correlation Matrix Lite

On the web view, show strongest relationships among configured tickers.

Resource cost:

- Low if it reuses pair-trading correlation calculations.

Value:

- Helps explain why certain pair-trading candidates appear.
- More useful on the web view than on the small device screen.

### Market Regime Indicator

Use configured major assets, usually BTC and ETH when present, as a rough risk-on/risk-off indicator.

Possible labels:

- Risk On
- Mixed
- Risk Off

Simple calculation:

- Check whether BTC and ETH are above or below their recent moving averages.
- If both are above, label risk-on.
- If both are below, label risk-off.
- Otherwise label mixed.

Resource cost:

- Very low.

Value:

- Useful market context.
- Less useful if BTC or ETH are not configured.

### History Health

Show whether each configured ticker has valid current and history data.

Possible labels:

- Live
- Price Only
- Missing History
- Stale

Resource cost:

- Very low.

Value:

- Excellent for debugging.
- Helps explain why pair trading or sparklines may be blank.

## Recommendation

Start with:

1. Volatility Score
2. Outlier Alert
3. History Health

These give the best mix of usefulness, clarity, and low resource cost. They also make the existing crypto and pair-trading screens easier to interpret.

After that, add web-only Correlation Matrix Lite. It is valuable, but the detail fits the browser better than the device screen.

## Implementation Notes

- Keep calculations in a separate include file, similar to `pair_trading.inc`.
- Reuse existing 30-day history arrays instead of fetching new data.
- Avoid long-running calculations inside the main loop.
- Use dirty flags so analysis recomputes only when prices, history, or ticker config changes.
- Keep labels short on the device and more explanatory in the web view.
- Do not present outputs as guaranteed trades or predictions.

## Acceptance Criteria

- Analysis works dynamically from the user's configured tickers.
- No additional API endpoint is required for the first version.
- Device remains responsive while analysis is shown.
- Web view provides more detail without increasing device screen clutter.
- Missing or incomplete history produces clear fallback labels.
