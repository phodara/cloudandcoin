# SD Card Minimal Changes

Historical note: this document records the original minimal SD-card integration approach. The current firmware has since been refactored into `src/main.cpp` plus source fragments under `src/app/`, and the crypto limit/behavior has grown beyond the early examples below.

This note captures the minimum code changes needed to add SD-card-based ticker loading without changing the page-swipe logic.

## Goal
- Initialize the onboard SD card
- Read `/crypto_tickers.txt` from the SD card root
- Parse up to 3 crypto definitions
- Use those tickers for the crypto screen
- Leave the existing swipe/touch navigation alone

## Hardware Configuration That Worked
- SD uses a dedicated SPI bus
- `SCK = 18`
- `MISO = 19`
- `MOSI = 23`
- `CS = 5`
- SD must use `HSPI` in code for this project

## Minimum Code Changes

### 1. Add SD includes and pin definitions
In `src/main.cpp`:

```cpp
#include <SD.h>

#define SD_CS_PIN   5
#define SD_SCK_PIN  18
#define SD_MISO_PIN 19
#define SD_MOSI_PIN 23
```

### 2. Add a dedicated SPI bus for SD
Do not reuse the display/touch SPI object.

```cpp
SPIClass sdSpi(HSPI);
```

### 3. Keep the existing display/touch SPI setup unchanged
Leave this alone:

```cpp
SPI.begin(14, 12, 13);
ts.begin();
ts.setRotation(1);
```

### 4. Initialize the SD card on its own bus
Add a helper that runs during setup after UI creation and before crypto data is used:

```cpp
sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
if (!SD.begin(SD_CS_PIN, sdSpi)) {
  // fall back to default tickers
}
```

## Problem Found During Integration
An earlier SD implementation used a dedicated pin set but created the SD bus with:

```cpp
SPIClass sdSpi(VSPI);
```

That version compiled and the SD card could be read, but it broke screen swipe behavior.

### Observed Symptom
- SD card loading worked
- screen swipe stopped working or became unreliable

### Root Cause
Even though the SD card used different pins, putting it on the wrong SPI host interfered with the touch/display path in this project.

The working display/touch code already depended on the existing SPI/touch setup, so the SD reader needed to be isolated not just by pins, but by SPI host as well.

### Resolution
Use:

```cpp
SPIClass sdSpi(HSPI);
```

with:

```cpp
sdSpi.begin(18, 19, 23, 5);
SD.begin(5, sdSpi);
```

This preserved the original swipe behavior while still allowing the SD card to initialize and read `/crypto_tickers.txt`.

### 5. Read `/crypto_tickers.txt`
Open the file from the SD root:

```cpp
File file = SD.open("/crypto_tickers.txt", FILE_READ);
```

Read one line at a time:
- trim whitespace
- ignore blank lines
- ignore comment lines starting with `#`
- stop after 3 configured entries

### 6. Map SD entries to app data
The current implementation supports two input styles:

- metadata lines for arbitrary coins
- legacy one-token lines for built-in defaults only

The metadata format is:

```text
SYMBOL|COINGECKO_ID|DECIMALS
```

The built-in table still exists for default fallback and legacy one-token lines:

```cpp
struct CryptoDefinition {
  const char *symbol;
  const char *coinGeckoId;
  int decimals;
};
```

Legacy built-in symbols:
- `BTC`
- `ETH`
- `ADA`

For metadata lines, the SD card now provides:
- display symbol
- CoinGecko ID
- display formatting

### 7. Fall back safely
If any SD step fails:
- keep the default crypto list
- do not modify swipe/page logic
- do not reconfigure the display SPI bus

## File Format
`/crypto_tickers.txt`

Primary format:

```text
SYMBOL|COINGECKO_ID|DECIMALS
```

Example:

```text
BTC|bitcoin|0
ETH|ethereum|0
ADA|cardano|3
```

Example with a custom coin:

```text
XLM|stellar|3
ETH|ethereum|0
ADA|cardano|3
```

Optional comments:

```text
# crypto list
BTC|bitcoin|0
ETH|ethereum|0
ADA|cardano|3
```

Notes:
- `SYMBOL` is the label shown on screen
- `COINGECKO_ID` is used for both current price and 30-day history
- `DECIMALS` controls displayed precision
- legacy one-token lines like `BTC` still work only for the built-in defaults
- older 4-field lines are still accepted, but the second field is ignored

## Important Constraint
The SD card code must not:
- change `currentPage`
- change `showPage()`
- replace swipe with tap
- alter the touch read path
- reuse the display SPI bus
- use the wrong SPI host for the SD reader

## Recommended Integration Order
1. Add SD init on dedicated SPI bus
2. Confirm the app still boots and swipe still works
3. Read the ticker file
4. Parse metadata into runtime crypto configs
5. Feed the parsed list into the existing crypto data/update path
