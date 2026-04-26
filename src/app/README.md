# App Source Fragments

`main.cpp` includes these files directly to keep the firmware in one C++ translation unit while separating the major responsibilities.

This is an intentionally conservative refactor for the Arduino/PlatformIO build:

- shared globals stay in `main.cpp`
- setup and loop orchestration stay in `main.cpp`
- implementation blocks live in focused fragments
- behavior should remain unchanged

Fragments:

- `helpers.inc`: common utility, config, SD card, battery, and formatting helpers
- `web.inc`: web editor, setup, remote view, and web route handlers
- `display_touch.inc`: LVGL display flush and touch input
- `crypto_data.inc`: crypto ticker loading and CoinGecko fetches
- `weather_data.inc`: OpenWeather fetches and forecast parsing
- `pair_trading.inc`: dynamic pair candidate scoring from configured crypto histories
- `runtime_ui.inc`: page switching, refresh orchestration, drawing, and screen updates
- `ui_build.inc`: LVGL styling and UI construction

If this is later converted to separate `.cpp` modules, add explicit headers and `extern` declarations instead of relying on include order.
