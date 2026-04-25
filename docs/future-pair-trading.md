# Future Pair Trading

This note captures a possible future third screen for showing crypto pair-trading candidates.

## Goal

Add a `Pair Trading` screen that ranks crypto pairs whose prices historically move together, but where one asset has temporarily diverged from the other.

The screen should surface statistically interesting opportunities, not guaranteed trades. Crypto relationships can break quickly because of token-specific news, exchange issues, liquidity changes, unlocks, regulatory events, or broader market shocks.

## Concept

Pair trading compares two related assets and looks for temporary divergence from their usual relationship.

Example candidate pairs:

- BTC / ETH
- SOL / AVAX
- LINK / UNI
- MATIC / OP
- DOGE / SHIB

When a pair diverges, the screen could suggest a directional bias such as:

- Long ETH / Short BTC
- Long AVAX / Short SOL
- No trade when the divergence is weak or the relationship is unstable

## Ranking Inputs

The scoring logic could consider:

- Correlation: whether the two assets historically move together.
- Spread or ratio divergence: whether one asset is currently unusually cheap or expensive versus the other.
- Z-score: how extreme the current divergence is compared with recent history.
- Mean-reversion history: whether past divergences usually closed again.
- Volatility-adjusted signal: whether the move looks meaningful instead of ordinary noise.
- Liquidity and market-cap filters: whether the assets are practical to trade and not distorted by thin volume.

## Example Screen

The device screen could show a compact ranked list:

```text
Pair       Signal    Z-Score  Corr   Bias
ETH/BTC    Strong    -2.1     0.86   Long ETH
SOL/AVAX   Medium     1.6     0.78   Long AVAX
LINK/UNI   Weak      -1.1     0.70   Watch
```

The web view could provide a richer detail view for each pair:

- Price ratio chart
- Historical spread chart
- Current deviation band
- Confidence score
- Plain-language explanation of why the pair is ranked

## Data Requirements

This feature would need more historical data than the current crypto ticker screen.

Potential data fields:

- symbol A and symbol B
- CoinGecko id for each asset
- current price for each asset
- historical price series for each asset
- rolling correlation
- rolling ratio or spread
- mean spread
- standard deviation of spread
- z-score
- liquidity or market-cap proxy

## Implementation Notes

When implementing this later:

- Treat this as a third top-level screen alongside weather and crypto.
- Keep the initial pair universe small and configurable.
- Prefer daily historical closes at first instead of high-frequency data.
- Calculate ranking server-side on the ESP32 only if memory and CPU are acceptable.
- If the device cannot handle the analysis comfortably, consider precomputed pair data from a lightweight endpoint or generated static data on the SD card.
- Keep the display compact: show only the top few candidates on the device and use the web view for detail.
- Label outputs as signals or candidates, not instructions to trade.

## Acceptance Criteria

- A third `Pair Trading` screen is available.
- Candidate pairs are ranked by a clear score.
- Each candidate shows pair, signal strength, z-score, correlation, and directional bias.
- Weak or unstable relationships can be filtered out.
- The web view can show additional pair detail without crowding the device display.
- The feature communicates uncertainty clearly and avoids presenting signals as guaranteed profitable trades.
