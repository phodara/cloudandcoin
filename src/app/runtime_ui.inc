// ---------------- Page switching ----------------
void showPage(int page) {
  currentPage = page;

  if (currentPage == 0) {
    lv_obj_clear_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);
#if ENABLE_TRADING_SIGNALS
    lv_obj_add_flag(signal_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_obj_invalidate(weather_page);
    weatherBadgesDirty = true;
  } else if (currentPage == 1) {
    lv_obj_clear_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);
#if ENABLE_TRADING_SIGNALS
    lv_obj_add_flag(signal_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_obj_invalidate(crypto_page);
    cryptoScrollOffset = 0;
    lastCryptoScrollMs = millis();
    renderCryptoWindow();
    set_status("Crypto updating");
    cryptoRefreshPending = true;
    if (!allCryptoHistoryReady() && !cryptoHistoryRefreshPending) startCryptoHistoryRefresh();
    else cryptoHistoryRefreshPending = false;
    cryptoSparklinesDirty = true;
  } else if (currentPage == 2) {
    lv_obj_clear_flag(pair_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
#if ENABLE_TRADING_SIGNALS
    lv_obj_add_flag(signal_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);
#endif
    lv_obj_invalidate(pair_page);
    pairTradingDirty = true;
    pairTradingRender();
    if (!allCryptoHistoryReady() && !cryptoHistoryRefreshPending) startCryptoHistoryRefresh();
#if ENABLE_TRADING_SIGNALS
  } else {
    lv_obj_clear_flag(signal_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(signal_page);
    tradingSignalsDirty = true;
    tradingSignalsRender();
    if (!allCryptoHistoryReady() && !cryptoHistoryRefreshPending) startCryptoHistoryRefresh();
#endif
  }

  lv_refr_now(nullptr);
  if (currentPage == 1 && !cryptoAutoScrollEnabled()) {
    drawCryptoSparklines();
  }
}

// ---------------- Tap ----------------
void showNextPageFromTouch() {
  lastTouchInteractionMs = millis();
  if (currentPage == 0) showPage(1);
  else if (currentPage == 1) showPage(2);
#if ENABLE_TRADING_SIGNALS
  else if (currentPage == 2) showPage(3);
#endif
  else showPage(0);
}

void handleTapToggle() {
  bool touchActive = false;
  int x = 0;
  int y = 0;
  touchActive = readMappedTouch(x, y);
  if (!touchActive) {
    if (!touchDown) return;

    int dx = abs(touchCurrentX - touchDownX);
    int dy = abs(touchCurrentY - touchDownY);
    unsigned long pressMs = millis() - touchDownMs;

#if TOUCH_DEBUG
    Serial.printf("Tap debug: dx=%d dy=%d pressMs=%lu\n", dx, dy, pressMs);
#endif

    if (touchNavigatedOnPress) {
#if TOUCH_DEBUG
      Serial.println("Tap debug: already handled on press");
#endif
    } else if (dx <= tapMoveThreshold && dy <= tapMoveThreshold &&
        pressMs >= tapMinMs && pressMs <= tapMaxMs) {
#if TOUCH_DEBUG
      Serial.println("Tap debug: accepted");
#endif
      showNextPageFromTouch();
    } else {
#if TOUCH_DEBUG
      Serial.println("Tap debug: rejected");
#endif
    }

    touchDown = false;
    touchNavigatedOnPress = false;
    lastTouchInteractionMs = millis();
    return;
  }

  lastTouchInteractionMs = millis();
  if (!touchDown) {
    touchDown = true;
    touchNavigatedOnPress = false;
    touchDownX = x;
    touchDownY = y;
    touchCurrentX = x;
    touchCurrentY = y;
    touchDownMs = millis();

    showNextPageFromTouch();
    touchNavigatedOnPress = true;
    return;
  }

  touchCurrentX = x;
  touchCurrentY = y;
}

// ---------------- Weather ----------------
void updateWeather() {
  float temp = NAN;
  float pressure = NAN;
  char cond[16];

  if (!fetchCurrentWeather(temp, pressure, cond, sizeof(cond))) {
    currentWeatherOk = false;
    currentWeatherTemp = NAN;
    currentWeatherPressure = NAN;
    copyText(currentWeatherCond, sizeof(currentWeatherCond), "unknown");
    currentPressureTrend = PRESSURE_TREND_SAME;
    lv_label_set_text(weather_title_label, "Weather");
    lv_label_set_text(weather_temp_label, "-- F");
    lv_label_set_text(weather_cond_label, "Unavailable");
    lv_label_set_text(weather_hi_label, "Hi: -- F");
    lv_label_set_text(weather_lo_label, "Lo: -- F");
    lv_label_set_text(weather_pressure_label, "Pressure: --");
    lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(weather_cond_label, lv_color_hex(0xCCCCCC), 0);
    weatherBadgesDirty = true;
    return;
  }

  copyText(currentWeatherCond, sizeof(currentWeatherCond), cond, "unknown");
  currentWeatherOk = true;
  currentWeatherTemp = temp;
  currentWeatherPressure = pressure;
  currentPressureTrend = evaluatePressureTrend(pressure);

  lv_label_set_text(weather_title_label, "Weather");
  lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0xAAAAAA), 0);

  if (isnan(temp)) {
    lv_label_set_text(weather_temp_label, "-- F");
  } else {
    char tempText[12];
    snprintf(tempText, sizeof(tempText), "%d F", (int)round(temp));
    lv_label_set_text(weather_temp_label, tempText);
  }
  lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFFFFF), 0);

  lv_label_set_text(weather_cond_label, cond);

  lv_color_t condColor = lv_color_hex(0xCCCCCC);
  if (containsIgnoreCase(cond, "clear") || containsIgnoreCase(cond, "sun")) condColor = lv_color_hex(0xF4B400);
  else if (containsIgnoreCase(cond, "cloud")) condColor = lv_color_hex(0xD9DDE3);
  else if (containsIgnoreCase(cond, "rain") || containsIgnoreCase(cond, "drizzle")) condColor = lv_color_hex(0x2D6BFF);
  else if (containsIgnoreCase(cond, "storm") || containsIgnoreCase(cond, "thunder")) condColor = lv_color_hex(0x8E44AD);
  else if (containsIgnoreCase(cond, "snow")) condColor = lv_color_hex(0xB0E0FF);

  lv_obj_set_style_text_color(weather_cond_label, condColor, 0);

  if (isnan(pressure)) {
    lv_label_set_text(weather_pressure_label, "Pressure: --");
  } else {
    char pText[24];
    snprintf(pText, sizeof(pText), "Pressure: %d hPa", (int)round(pressure));
    lv_label_set_text(weather_pressure_label, pText);
    prevWeatherPressure = pressure;
  }

  if (todayHiLoOk) {
    char hiText[16];
    char loText[16];
    snprintf(hiText, sizeof(hiText), "Hi: %d F", todayHigh);
    snprintf(loText, sizeof(loText), "Lo: %d F", todayLow);
    lv_label_set_text(weather_hi_label, hiText);
    lv_label_set_text(weather_lo_label, loText);
  } else {
    lv_label_set_text(weather_hi_label, "Hi: -- F");
    lv_label_set_text(weather_lo_label, "Lo: -- F");
  }

  weatherBadgesDirty = true;
}

void updateForecastLabels() {
  for (int i = 0; i < 4; i++) {
    lv_obj_set_style_text_color(forecast_day_label[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_color(forecast_temp_label[i], lv_color_hex(0xF4B400), 0);
    lv_obj_set_style_text_color(forecast_cond_label[i], lv_color_hex(0xCCCCCC), 0);

    if (forecast_ok) {
      char temps[16];
      lv_label_set_text(forecast_day_label[i], forecast[i].day);
      snprintf(temps, sizeof(temps), "%d/%dF", forecast[i].high, forecast[i].low);
      lv_label_set_text(forecast_temp_label[i], temps);
      lv_label_set_text(forecast_cond_label[i], forecast[i].cond);
    } else {
      lv_label_set_text(forecast_day_label[i], "---");
      lv_label_set_text(forecast_temp_label[i], "--/--");
      lv_label_set_text(forecast_cond_label[i], "N/A");
    }
  }
  weatherBadgesDirty = true;
}

// ---------------- Crypto current price ----------------
void updatePrice(lv_obj_t *label, const char *sym, float currentVal, float prevVal, int decimals) {
  if (!label) return;

  if (isnan(currentVal)) {
    char txt[16];
    snprintf(txt, sizeof(txt), "%s  N/A", sym);
    lv_label_set_text(label, txt);
    lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
    return;
  }

  const char *arrow = "";
  uint32_t colorHex = 0xFFFFFF;

  if (!isnan(prevVal)) {
    if (currentVal < prevVal) {
      colorHex = 0xFF0000;
      arrow = " v";
    } else if (currentVal > prevVal) {
      colorHex = 0x00FF00;
      arrow = " ^";
    } else {
      arrow = " -";
    }
  } else {
    arrow = " -";
  }

  char priceBuf[24];
  dtostrf(currentVal, 0, decimals, priceBuf);
  char *trimmedPrice = priceBuf;
  while (*trimmedPrice == ' ') ++trimmedPrice;

  char txt[40];
  snprintf(txt, sizeof(txt), "%s  $%s%s", sym, trimmedPrice, arrow);
  lv_label_set_text(label, txt);
  lv_obj_set_style_text_color(label, lv_color_hex(colorHex), 0);
}

void renderCryptoWindow() {
  const bool showSparkBoxes = !cryptoAutoScrollEnabled();
  for (int slot = 0; slot < CRYPTO_VISIBLE_ROWS; slot++) {
    lv_obj_t *label = cryptoValueLabelAt(slot);
    lv_obj_t *box = cryptoBoxAt(slot);
    int activeIndex = cryptoActiveIndexForSlot(slot);

    if (activeIndex < 0) {
      if (label) {
        lv_label_set_text(label, "");
        lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
      }
      if (box) {
        lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
      }
      continue;
    }

    if (label) lv_obj_clear_flag(label, LV_OBJ_FLAG_HIDDEN);
    if (box) {
      if (showSparkBoxes) lv_obj_clear_flag(box, LV_OBJ_FLAG_HIDDEN);
      else lv_obj_add_flag(box, LV_OBJ_FLAG_HIDDEN);
    }

    updatePrice(
      label,
      activeCryptos[activeIndex].symbol,
      currentCryptoValues[activeIndex],
      previousCryptoValues[activeIndex],
      activeCryptos[activeIndex].decimals
    );
  }

  cryptoSparklinesDirty = true;
}

void updateCrypto() {
  lv_label_set_text(crypto_title_label, "Crypto");
  lv_obj_set_style_text_color(crypto_title_label, lv_color_hex(0xAAAAAA), 0);

  float latestValues[MAX_ACTIVE_CRYPTO_COUNT];
  fetchCurrentPricesUsd(latestValues, configuredCryptoCount);

  for (int i = 0; i < configuredCryptoCount; i++) {
    if (!isnan(latestValues[i])) {
      previousCryptoValues[i] = currentCryptoValues[i];
      currentCryptoValues[i] = latestValues[i];
    }
  }

  renderCryptoWindow();
}

// ---------------- Background data worker ----------------
void dataWorkerTask(void *param) {
  (void)param;

  for (;;) {
    DataJob job;
    if (xQueueReceive(dataJobQueue, &job, portMAX_DELAY) != pdTRUE) continue;

    if (job.type == DATA_JOB_CRYPTO_PRICES) {
      float latestValues[MAX_ACTIVE_CRYPTO_COUNT];
      bool ok = fetchCurrentPricesUsd(latestValues, job.count);

      if (xSemaphoreTake(dataWorkerMutex, portMAX_DELAY) == pdTRUE) {
        int count = job.count;
        if (count < 0) count = 0;
        if (count > MAX_ACTIVE_CRYPTO_COUNT) count = MAX_ACTIVE_CRYPTO_COUNT;
        stagedCryptoPriceCount = count;
        for (int i = 0; i < count; i++) stagedCryptoValues[i] = latestValues[i];
        stagedCryptoPriceRateLimited = cryptoPriceResultRateLimited;
        cryptoPriceResultOk = ok;
        cryptoPriceResultReady = true;
        xSemaphoreGive(dataWorkerMutex);
      }
    } else if (job.type == DATA_JOB_CRYPTO_HISTORY) {
      float history[HISTORY_POINTS];
      int count = 0;
      bool ok = fetchHistory30(job.coinGeckoId, history, count);

      if (xSemaphoreTake(dataWorkerMutex, portMAX_DELAY) == pdTRUE) {
        stagedCryptoHistoryIndex = job.index;
        stagedCryptoHistoryCount = count;
        for (int i = 0; i < HISTORY_POINTS; i++) stagedCryptoHistory[i] = history[i];
        cryptoHistoryResultOk = ok;
        cryptoHistoryResultReady = true;
        xSemaphoreGive(dataWorkerMutex);
      }
    }
  }
}

void startDataWorker() {
  if (dataJobQueue && dataWorkerMutex && dataWorkerTaskHandle) return;

  dataWorkerMutex = xSemaphoreCreateMutex();
  dataJobQueue = xQueueCreate(4, sizeof(DataJob));
  if (!dataWorkerMutex || !dataJobQueue) {
    Serial.println("Data worker: failed to allocate queue or mutex");
    return;
  }

  BaseType_t ok = xTaskCreatePinnedToCore(
    dataWorkerTask,
    "data-worker",
    8192,
    nullptr,
    1,
    &dataWorkerTaskHandle,
    0
  );

  if (ok != pdPASS) {
    Serial.println("Data worker: task create failed");
    dataWorkerTaskHandle = nullptr;
  }
}

bool queueCryptoPriceRefresh() {
  if (!dataJobQueue || !dataWorkerMutex) return false;
  if (cryptoCurrentBackoffActive()) return false;

  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;
  bool alreadyBusy = cryptoPriceJobQueued || cryptoPriceResultReady;
  if (!alreadyBusy) cryptoPriceJobQueued = true;
  xSemaphoreGive(dataWorkerMutex);
  if (alreadyBusy) return true;

  DataJob job = {};
  job.type = DATA_JOB_CRYPTO_PRICES;
  job.count = configuredCryptoCount;
  if (xQueueSendToFront(dataJobQueue, &job, 0) == pdTRUE) return true;

  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    cryptoPriceJobQueued = false;
    xSemaphoreGive(dataWorkerMutex);
  }
  return false;
}

bool queueCryptoHistoryRefresh(int index) {
  if (!dataJobQueue || !dataWorkerMutex) return false;
  if (index < 0 || index >= configuredCryptoCount) return false;

  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) != pdTRUE) return false;
  bool pricePending = cryptoPriceJobQueued || cryptoPriceResultReady;
  bool alreadyBusy = pricePending || cryptoHistoryJobQueued || cryptoHistoryResultReady;
  if (!alreadyBusy) cryptoHistoryJobQueued = true;
  xSemaphoreGive(dataWorkerMutex);
  if (alreadyBusy) return true;

  DataJob job = {};
  job.type = DATA_JOB_CRYPTO_HISTORY;
  job.index = index;
  snprintf(job.coinGeckoId, sizeof(job.coinGeckoId), "%s", activeCryptos[index].coinGeckoId);
  if (xQueueSend(dataJobQueue, &job, 0) == pdTRUE) return true;

  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    cryptoHistoryJobQueued = false;
    xSemaphoreGive(dataWorkerMutex);
  }
  return false;
}

bool dataWorkerBusy() {
  if (!dataWorkerMutex) return false;
  bool busy = false;
  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
    busy = cryptoPriceJobQueued || cryptoPriceResultReady ||
           cryptoHistoryJobQueued || cryptoHistoryResultReady;
    xSemaphoreGive(dataWorkerMutex);
  }
  return busy;
}

void finishCryptoHistoryStep() {
  cryptoSparklinesDirty = true;
  pairTradingDirty = true;
  tradingSignalsDirty = true;
  cryptoHistoryRefreshIndex++;

  if (cryptoHistoryRefreshIndex >= configuredCryptoCount) {
    if (allCryptoHistoryReady()) {
      cryptoHistoryRefreshPending = false;
      cryptoHistoryRefreshIndex = -1;
      cryptoHistoryRetryMissingOnly = false;
      lastHistoryRefresh = millis();
      lastCryptoPriceRefresh = lastHistoryRefresh;
      lastCryptoWebRefresh = lastHistoryRefresh;
      if (currentPage == 1) set_status("Updated");
    } else {
      cryptoHistoryRetryMissingOnly = true;
      cryptoHistoryRefreshIndex = 0;
      nextCryptoHistoryRetryMs = millis() + cryptoHistoryRetryIntervalMs();
      if (currentPage == 1) set_status("Retry history...");
    }
  }
}

void applyDataWorkerResults() {
  if (!dataWorkerMutex) return;

  bool havePrice = false;
  bool priceOk = false;
  bool priceRateLimited = false;
  int priceCount = 0;
  float priceValues[MAX_ACTIVE_CRYPTO_COUNT];
  bool haveHistory = false;
  bool historyOk = false;
  int historyIndex = -1;
  float historyValues[HISTORY_POINTS];

  if (xSemaphoreTake(dataWorkerMutex, pdMS_TO_TICKS(5)) != pdTRUE) return;

  if (cryptoPriceResultReady) {
    havePrice = true;
    priceOk = cryptoPriceResultOk;
    priceRateLimited = stagedCryptoPriceRateLimited;
    priceCount = stagedCryptoPriceCount;
    if (priceCount > MAX_ACTIVE_CRYPTO_COUNT) priceCount = MAX_ACTIVE_CRYPTO_COUNT;
    for (int i = 0; i < priceCount; i++) priceValues[i] = stagedCryptoValues[i];
    cryptoPriceResultReady = false;
    cryptoPriceJobQueued = false;
  }

  if (cryptoHistoryResultReady) {
    haveHistory = true;
    historyOk = cryptoHistoryResultOk;
    historyIndex = stagedCryptoHistoryIndex;
    for (int i = 0; i < HISTORY_POINTS; i++) historyValues[i] = stagedCryptoHistory[i];
    cryptoHistoryResultReady = false;
    cryptoHistoryJobQueued = false;
  }

  xSemaphoreGive(dataWorkerMutex);

  if (havePrice) {
    bool hasDisplayedPrice = false;
    for (int i = 0; i < priceCount; i++) {
      if (!isnan(priceValues[i])) {
        previousCryptoValues[i] = currentCryptoValues[i];
        currentCryptoValues[i] = priceValues[i];
      }
      if (!isnan(currentCryptoValues[i])) hasDisplayedPrice = true;
    }
    renderCryptoWindow();
    pairTradingDirty = true;
    tradingSignalsDirty = true;
    if (currentPage == 2) pairTradingRender();
    if (currentPage == 3) tradingSignalsRender();
    if (priceOk) {
      nextCryptoCurrentRetryMs = 0;
      set_status("Updated");
    } else if (priceRateLimited) {
      nextCryptoCurrentRetryMs = millis() + cryptoCurrentRetryIntervalMs();
      Serial.printf("CG current batch: rate limited, retry after %d minute(s)\n", deviceConfig.cgCurrentRetryMinutes);
      if (hasDisplayedPrice) set_status("CG wait");
      else set_status("CG rate limit");
    } else if (hasDisplayedPrice) set_status("Cached prices");
    else set_status("No prices");
  }

  if (haveHistory && historyIndex >= 0 && historyIndex < configuredCryptoCount) {
    if (historyOk) {
      for (int i = 0; i < HISTORY_POINTS; i++) cryptoHistory[historyIndex][i] = historyValues[i];
      if (isnan(previousCryptoValues[historyIndex]) && !isnan(currentCryptoValues[historyIndex])) {
        for (int i = HISTORY_POINTS - 2; i >= 0; i--) {
          if (!isnan(historyValues[i]) && historyValues[i] > 0.0f) {
            previousCryptoValues[historyIndex] = historyValues[i];
            break;
          }
        }
      }
    }
    if (historyOk || !cryptoHistoryOk[historyIndex]) cryptoHistoryOk[historyIndex] = historyOk;
    renderCryptoWindow();
    finishCryptoHistoryStep();
  }
}

void updateCryptoAutoScroll() {
  if (currentPage != 1 || !cryptoAutoScrollEnabled()) return;

  unsigned long now = millis();
  if (now - lastCryptoScrollMs < CRYPTO_SCROLL_INTERVAL_MS) return;
  lastCryptoScrollMs = now;

  if (cryptoScrollOffset < cryptoMaxScrollOffset()) cryptoScrollOffset++;
  else cryptoScrollOffset = 0;

  renderCryptoWindow();
}

// ---------------- Sparkline drawing ----------------
uint16_t sparkColor(float vals[], bool ok) {
  if (!ok) return TFT_DARKGREY;
  float first = vals[0];
  float last = vals[HISTORY_POINTS - 1];
  if (last > first) return TFT_GREEN;
  if (last < first) return TFT_RED;
  return TFT_LIGHTGREY;
}

void drawSparklineInBox(int screenX, int screenY, int w, int h, float vals[], bool ok, uint16_t placeholderColor) {
  int ix = screenX + 1;
  int iy = screenY + 1;
  int iw = w - 2;
  int ih = h - 2;

  tft.fillRect(ix, iy, iw, ih, TFT_BLACK);

  int gx = ix + 2;
  int gy = iy + 2;
  int gw = iw - 4;
  int gh = ih - 4;

  if (!ok) {
    for (int i = 0; i < HISTORY_POINTS - 1; i++) {
      int x1 = gx + (i * (gw - 1)) / (HISTORY_POINTS - 1);
      int x2 = gx + ((i + 1) * (gw - 1)) / (HISTORY_POINTS - 1);
      int y1 = gy + gh / 2 + ((i % 2 == 0) ? -4 : 4);
      int y2 = gy + gh / 2 + (((i + 1) % 2 == 0) ? -4 : 4);
      tft.drawLine(x1, y1, x2, y2, placeholderColor);
    }
    return;
  }

  float vmin = vals[0], vmax = vals[0];
  for (int i = 1; i < HISTORY_POINTS; i++) {
    if (!isnan(vals[i])) {
      if (vals[i] < vmin) vmin = vals[i];
      if (vals[i] > vmax) vmax = vals[i];
    }
  }

  float span = vmax - vmin;
  if (span < 0.000001f || isnan(span)) span = 1.0f;

  uint16_t color = sparkColor(vals, ok);
  int px = gx, py = gy + gh / 2;

  for (int i = 0; i < HISTORY_POINTS; i++) {
    float v = isnan(vals[i]) ? vals[0] : vals[i];
    int sx = gx + (i * (gw - 1)) / (HISTORY_POINTS - 1);
    int sy = gy + gh - 1 - (int)round(((v - vmin) / span) * (gh - 1));
    if (sy < gy) sy = gy;
    if (sy > gy + gh - 1) sy = gy + gh - 1;
    if (i > 0) tft.drawLine(px, py, sx, sy, color);
    px = sx;
    py = sy;
  }
}

void drawSparklineInLvglBox(lv_obj_t *box, float vals[], bool ok, uint16_t placeholderColor) {
  if (!box || !vals) return;
  lv_area_t a;
  lv_obj_get_coords(box, &a);
  drawSparklineInBox(a.x1, a.y1, a.x2 - a.x1 + 1, a.y2 - a.y1 + 1, vals, ok, placeholderColor);
}

void drawCryptoBadges() {
  if (currentPage != 1) return;
  if (cryptoAutoScrollEnabled()) return;

  tft.fillRect(PAGE_X + 32, PAGE_Y + 12, 34, 200, TFT_BLACK);

  for (int slot = 0; slot < CRYPTO_VISIBLE_ROWS; slot++) {
    int activeIndex = cryptoActiveIndexForSlot(slot);
    if (activeIndex < 0) continue;

    int cy = PAGE_Y + 37 + (slot * 50);
    uint16_t fill = activeCryptos[activeIndex].badgeColor;
    uint16_t text = (fill == TFT_BLUE) ? TFT_WHITE : TFT_BLACK;
    char badge[2] = {activeCryptos[activeIndex].badgeChar, '\0'};

    tft.fillCircle(PAGE_X + 46, cy, 12, fill);
    tft.setTextColor(text, fill);
    tft.drawCentreString(badge, PAGE_X + 46, cy - 6, 2);
  }
}

void drawCryptoSparklines() {
  if (currentPage != 1) return;
  if (cryptoAutoScrollEnabled()) return;

  drawCryptoBadges();
  for (int slot = 0; slot < CRYPTO_VISIBLE_ROWS; slot++) {
    int boxX = PAGE_X + lv_obj_get_style_pad_left(crypto_page, 0) + CRYPTO_BOX_X;
    int boxY = PAGE_Y + lv_obj_get_style_pad_top(crypto_page, 0) + CRYPTO_ROW_BOX_Y[slot];
    int activeIndex = cryptoActiveIndexForSlot(slot);
    if (activeIndex < 0) {
      tft.fillRect(boxX + 1, boxY + 1, BOX_W - 2, BOX_H - 2, TFT_BLACK);
      continue;
    }
    drawSparklineInBox(
      boxX,
      boxY,
      BOX_W,
      BOX_H,
      cryptoHistoryAt(activeIndex),
      *cryptoHistoryOkAt(activeIndex),
      activeCryptos[activeIndex].sparkPlaceholderColor
    );
  }

  cryptoSparklinesDirty = false;
}

void drawWeatherBadgeCircle(int cx, int cy, int r, const char *code, const char *cond) {
  uint16_t fill = weatherIconColor(cond);
  uint16_t text = (fill == TFT_WHITE || fill == TFT_YELLOW || fill == TFT_LIGHTGREY) ? TFT_BLACK : TFT_WHITE;

  tft.fillCircle(cx, cy, r, fill);
  tft.drawCircle(cx, cy, r, TFT_DARKGREY);
  tft.setTextColor(text, fill);
  tft.drawCentreString(code, cx, cy - 6, 2);
}

PressureTrend evaluatePressureTrend(float currentPressure) {
  if (isnan(currentPressure) || isnan(prevWeatherPressure)) return PRESSURE_TREND_SAME;

  const float delta = currentPressure - prevWeatherPressure;
  const float stableThresholdHpa = 1.0f;

  if (delta >= stableThresholdHpa) return PRESSURE_TREND_BETTER;
  if (delta <= -stableThresholdHpa) return PRESSURE_TREND_WORSE;
  return PRESSURE_TREND_SAME;
}

void drawPressureTrendFace(int cx, int cy, int r, PressureTrend trend) {
  uint16_t fill = TFT_WHITE;
  if (trend == PRESSURE_TREND_BETTER) fill = TFT_YELLOW;
  else if (trend == PRESSURE_TREND_WORSE) fill = TFT_RED;

  tft.fillCircle(cx, cy, r, fill);
  tft.drawCircle(cx, cy, r, TFT_DARKGREY);

  uint16_t feature = (trend == PRESSURE_TREND_WORSE) ? TFT_WHITE : TFT_BLACK;
  int eyeOffsetX = r / 3;
  int eyeOffsetY = r / 4;

  tft.fillCircle(cx - eyeOffsetX, cy - eyeOffsetY, 1, feature);
  tft.fillCircle(cx + eyeOffsetX, cy - eyeOffsetY, 1, feature);

  if (trend == PRESSURE_TREND_BETTER) {
    tft.drawLine(cx - 4, cy + 2, cx, cy + 5, feature);
    tft.drawLine(cx, cy + 5, cx + 4, cy + 2, feature);
  } else if (trend == PRESSURE_TREND_WORSE) {
    tft.drawLine(cx - 4, cy + 5, cx, cy + 2, feature);
    tft.drawLine(cx, cy + 2, cx + 4, cy + 5, feature);
  } else {
    tft.drawLine(cx - 4, cy + 4, cx + 4, cy + 4, feature);
  }
}

void drawWeatherBadges() {
  if (currentPage != 0) return;

  tft.fillRect(PAGE_X + 355, PAGE_Y + 10, 85, 70, TFT_BLACK);
  drawWeatherBadgeCircle(PAGE_X + 396, PAGE_Y + 42, 24, weatherIconCode(currentWeatherCond), currentWeatherCond);
  tft.fillRect(PAGE_X + 410, PAGE_Y + 82, 26, 26, TFT_BLACK);
  tft.fillRect(PAGE_X + 250, PAGE_Y + 66, 28, 34, TFT_BLACK);
  drawPressureTrendFace(PAGE_X + 264, PAGE_Y + 79, 10, currentPressureTrend);

  for (int i = 0; i < 4; i++) {
    lv_area_t a;
    lv_obj_get_coords(forecast_box[i], &a);

    int bx = a.x1;
    int by = a.y1;

    tft.fillRect(bx + 60, by + 8, 28, 28, TFT_BLACK);

    if (forecast_ok) {
      drawWeatherBadgeCircle(bx + 74, by + 22, 12, weatherIconCode(forecast[i].cond), forecast[i].cond);
    } else {
      drawWeatherBadgeCircle(bx + 74, by + 22, 12, "N/A", "unknown");
    }
  }

  weatherBadgesDirty = false;
}

// ---------------- Refresh ----------------
void updateHistorySparklines() {
  int count = 0;

  for (int i = 0; i < configuredCryptoCount; i++) {
    bool ok = fetchHistory30(activeCryptos[i].coinGeckoId, cryptoHistoryAt(i), count);
    if (ok || !*cryptoHistoryOkAt(i)) *cryptoHistoryOkAt(i) = ok;
    if (i < configuredCryptoCount - 1) delay(700);
  }

  lastHistoryRefresh = millis();
  cryptoSparklinesDirty = true;
  pairTradingDirty = true;
  tradingSignalsDirty = true;
}

void startCryptoHistoryRefresh() {
  cryptoHistoryRefreshPending = configuredCryptoCount > 0;
  cryptoHistoryRefreshIndex = cryptoHistoryRefreshPending ? 0 : -1;
  cryptoHistoryRetryMissingOnly = false;
  lastCryptoHistoryStepMs = 0;
  nextCryptoHistoryRetryMs = 0;
  pairTradingDirty = true;
  tradingSignalsDirty = true;
}

void stepCryptoHistoryRefresh() {
  if (!cryptoHistoryRefreshPending || cryptoHistoryRefreshIndex < 0) return;

  unsigned long now = millis();
  if (nextCryptoHistoryRetryMs != 0 && (long)(now - nextCryptoHistoryRetryMs) < 0) {
    return;
  }
  nextCryptoHistoryRetryMs = 0;

  if (lastCryptoHistoryStepMs != 0 && (now - lastCryptoHistoryStepMs) < cryptoHistoryStepIntervalMs()) {
    return;
  }

  if (cryptoHistoryRetryMissingOnly) {
    while (cryptoHistoryRefreshIndex < configuredCryptoCount &&
           *cryptoHistoryOkAt(cryptoHistoryRefreshIndex)) {
      cryptoHistoryRefreshIndex++;
    }
  }

  if (cryptoHistoryRefreshIndex >= configuredCryptoCount) {
    cryptoHistoryRefreshPending = false;
    cryptoHistoryRefreshIndex = -1;
    cryptoHistoryRetryMissingOnly = false;
    lastHistoryRefresh = millis();
    lastCryptoPriceRefresh = lastHistoryRefresh;
    lastCryptoWebRefresh = lastHistoryRefresh;
    pairTradingDirty = true;
    tradingSignalsDirty = true;
    if (currentPage == 1) set_status("Updated");
    return;
  }

  if (currentPage == 1) set_status("Loading history...");

  if (queueCryptoHistoryRefresh(cryptoHistoryRefreshIndex)) lastCryptoHistoryStepMs = now;
}

void refreshAll(bool announceStatus) {
  if (WiFi.status() != WL_CONNECTED) {
    if (announceStatus) set_status("WiFi disconnected");
    return;
  }

  if (announceStatus) set_status("Updating...");

  if (millis() - lastForecastRefresh >= forecastRefreshIntervalMs || !forecast_ok) {
    bool ok = fetchForecast4();
    updateForecastLabels();
    if (ok) lastForecastRefresh = millis();
  }

  updateWeather();
  if (announceStatus) set_status("Updated");

  if (currentPage == 0) weatherBadgesDirty = true;
}

void showSetupInstructions() {
  if (!setup_page || !setup_message_label) return;

  const char *setupText =
    "Initial Setup\n\n"
    "1. Join the temporary network:\n"
    "   cloudandcoin-setup\n\n"
    "2. Open:\n"
    "   192.168.4.1/secrets\n\n"
    "3. Add your local network settings.";

  lv_label_set_text(setup_message_label, setupText);
  lv_obj_clear_flag(setup_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pair_page, LV_OBJ_FLAG_HIDDEN);
#if ENABLE_TRADING_SIGNALS
  lv_obj_add_flag(signal_page, LV_OBJ_FLAG_HIDDEN);
#endif
  lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(pair_title_label, LV_OBJ_FLAG_HIDDEN);
#if ENABLE_TRADING_SIGNALS
  lv_obj_add_flag(signal_title_label, LV_OBJ_FLAG_HIDDEN);
#endif
  lv_refr_now(nullptr);
}
