# Future Crypto Scaling

This note captures the intended future design for supporting larger crypto ticker lists without increasing display, memory, or API overhead too much.

## Goal

Support larger ticker lists, such as 20-30 configured tickers, while keeping the device display readable and the web view useful.

The current 1-4 ticker layout with sparklines should remain available. Larger ticker lists should switch to a lighter one-line row layout.

## Display Modes

### 1-4 Tickers

Keep the existing fixed crypto layout with sparklines.

Expected behavior:

- Show up to 4 tickers on the device screen at once.
- Keep 30-day sparkline history for each visible configured ticker.
- Keep the existing color-coded up/down/flat price movement indicator.
- Web view can continue showing the richer compact list and may include sparkline data if already available.

### 5 Or More Tickers

Disable sparklines and history fetches. Use one compact text row per ticker.

Example row:

```text
BTC  $94,250  ^  H 95,100  L 92,800
```

Device behavior:

- Show 4 ticker rows at a time.
- Page or auto-scroll through the configured ticker list.
- Keep each row to one line where possible.
- Use a current trend indicator:
  - `^` when current price is above the previous fetched price
  - `v` when current price is below the previous fetched price
  - `-` when flat or no comparison baseline exists
- Show day high and day low on the same row.

Web behavior:

- Use the same one-line row data:

```text
BTC  $94,250  ^  H 95,100  L 92,800
ETH  $3,120   v  H 3,180   L 3,090
```

- Render all configured tickers in a fixed-height scrollable crypto panel.
- Prefer a scrollable `div` over an iframe unless iframe isolation is specifically needed later.

## Data Model

For larger lists, store only lightweight per-ticker fields:

- symbol
- CoinGecko id
- decimal places
- current price
- previous fetched price
- day high
- day low
- trend indicator or enough data to calculate it

Avoid storing 30-day history arrays when ticker count is greater than 4.

## Fetching Strategy

Live price data should be fetched in batches when possible.

For larger ticker lists:

- Avoid one history request per ticker.
- Avoid sparkline history refreshes.
- Fetch current prices, day high, day low, and price movement data using the lightest CoinGecko endpoint that supports the required fields.
- If day high/day low require a heavier endpoint, fetch them less often than current price.
- Keep existing last-good-value behavior if an API request fails.

## Implementation Notes

When implementing this later:

- Increase `MAX_ACTIVE_CRYPTO_COUNT` beyond 10 only after reviewing RAM usage.
- Keep `CRYPTO_VISIBLE_ROWS` at 4 for the device screen unless the UI is redesigned.
- Gate sparkline/history allocation and fetch behavior on `configuredCryptoCount <= 4`.
- For `configuredCryptoCount > 4`, skip history refreshes and sparkline drawing entirely.
- Update `/view` so the web crypto panel can scroll through all configured tickers.
- Keep the existing `/crypto_tickers.txt` format unless a new field is truly needed.

## Acceptance Criteria

- 1-4 configured tickers still show sparklines on the device.
- 5 or more configured tickers show compact one-line rows without sparklines.
- The device shows 4 rows at a time and pages or auto-scrolls through larger lists.
- The web view shows all configured tickers in a scrollable crypto panel.
- Up/down indicators work from current price versus previous fetched price.
- Day high and day low are shown on each row when available.
- Failed crypto fetches keep the last good displayed values.
