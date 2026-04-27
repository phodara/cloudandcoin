// ---------------- Trading signals ----------------
// Lightweight informational scorecard based on existing current price and 30-day history.

#if ENABLE_TRADING_SIGNALS

struct TradingSignalResult {
  int score;
  float latestPrice;
  const char *action;
  const char *reason;
};

float tradingSignalLatestPrice(int index) {
  if (index < 0 || index >= configuredCryptoCount) return NAN;
  if (!isnan(currentCryptoValues[index]) && currentCryptoValues[index] > 0.0f) return currentCryptoValues[index];
  if (cryptoHistoryOk[index] && cryptoHistory[index][HISTORY_POINTS - 1] > 0.0f) {
    return cryptoHistory[index][HISTORY_POINTS - 1];
  }
  return NAN;
}

float tradingSignalAverage(const float history[], int start, int endExclusive) {
  float sum = 0.0f;
  int count = 0;

  if (!history) return NAN;
  if (start < 0) start = 0;
  if (endExclusive > HISTORY_POINTS) endExclusive = HISTORY_POINTS;

  for (int i = start; i < endExclusive; i++) {
    if (!isnan(history[i]) && history[i] > 0.0f) {
      sum += history[i];
      count++;
    }
  }

  if (count == 0) return NAN;
  return sum / count;
}

bool tradingSignalRange(const float history[], float &low, float &high) {
  if (!history) return false;

  low = NAN;
  high = NAN;

  for (int i = 0; i < HISTORY_POINTS; i++) {
    float value = history[i];
    if (isnan(value) || value <= 0.0f) continue;
    if (isnan(low) || value < low) low = value;
    if (isnan(high) || value > high) high = value;
  }

  return !isnan(low) && !isnan(high) && high > low;
}

TradingSignalResult calculateTradingSignal(int index) {
  TradingSignalResult result;
  result.score = 0;
  result.latestPrice = tradingSignalLatestPrice(index);
  result.action = "WAIT";
  result.reason = "Missing history";

  if (index < 0 || index >= configuredCryptoCount || !cryptoHistoryOk[index]) return result;

  const float *history = cryptoHistory[index];
  float latest = result.latestPrice;
  if (isnan(latest) || latest <= 0.0f) {
    result.reason = "Missing price";
    return result;
  }

  float first = history[0];
  float prev = history[HISTORY_POINTS - 2];
  float lastHistory = history[HISTORY_POINTS - 1];
  if (isnan(first) || isnan(prev) || isnan(lastHistory) || first <= 0.0f || prev <= 0.0f || lastHistory <= 0.0f) {
    return result;
  }

  int score = 0;
  const char *reason = "Mixed";

  float avg7 = tradingSignalAverage(history, HISTORY_POINTS - 7, HISTORY_POINTS);
  float avg14 = tradingSignalAverage(history, HISTORY_POINTS - 14, HISTORY_POINTS);
  float avg30 = tradingSignalAverage(history, 0, HISTORY_POINTS);
  float low = NAN;
  float high = NAN;
  bool hasRange = tradingSignalRange(history, low, high);

  float thirtyDayReturn = (latest / first) - 1.0f;
  float oneStepReturn = (latest / prev) - 1.0f;

  if (!isnan(avg7) && latest > avg7) score++;
  else if (!isnan(avg7) && latest < avg7) score--;

  if (!isnan(avg14) && !isnan(avg30) && avg14 > avg30) score++;
  else if (!isnan(avg14) && !isnan(avg30) && avg14 < avg30) score--;

  if (thirtyDayReturn > 0.05f) score++;
  else if (thirtyDayReturn < -0.05f) score--;

  if (oneStepReturn > 0.02f) score++;
  else if (oneStepReturn < -0.02f) score--;

  if (hasRange) {
    float span = high - low;
    float upperBand = high - (span * 0.12f);
    float lowerBand = low + (span * 0.12f);
    if (latest >= upperBand) {
      score++;
      reason = "Near 30d high";
    } else if (latest <= lowerBand) {
      score--;
      reason = "Near 30d low";
    }
  }

  if (score >= 3) {
    result.action = "BUY";
    if (strcmp(reason, "Mixed") == 0) reason = "Trend up";
  } else if (score >= 1) {
    result.action = "WATCH";
    if (strcmp(reason, "Mixed") == 0) reason = "Momentum up";
  } else if (score <= -3) {
    result.action = "SELL";
    if (strcmp(reason, "Mixed") == 0) reason = "Trend down";
  } else if (score <= -1) {
    result.action = "WATCH";
    if (strcmp(reason, "Mixed") == 0) reason = "Momentum down";
  } else {
    result.action = "HOLD";
  }

  result.score = score;
  result.reason = reason;
  return result;
}

void tradingSignalsRender() {
  if (!signal_title_label) return;

  lv_label_set_text(signal_title_label, "Signals");
  lv_obj_set_style_text_color(signal_title_label, lv_color_hex(0xAAAAAA), 0);

  if (configuredCryptoCount <= 0) {
    for (int i = 0; i < SIGNAL_VISIBLE_ROWS; i++) {
      lv_obj_t *label = signal_value_labels[i];
      if (!label) continue;
      lv_label_set_text(label, i == 0 ? "Add crypto tickers" : "");
      lv_obj_set_style_text_color(label, lv_color_hex(0xAAAAAA), 0);
    }
    tradingSignalsDirty = false;
    return;
  }

  for (int i = 0; i < SIGNAL_VISIBLE_ROWS; i++) {
    lv_obj_t *label = signal_value_labels[i];
    if (!label) continue;

    if (i >= configuredCryptoCount) {
      lv_label_set_text(label, "");
      continue;
    }

    TradingSignalResult signal = calculateTradingSignal(i);
    char priceText[24];
    formatCryptoPriceText(signal.latestPrice, activeCryptos[i].decimals, priceText, sizeof(priceText));

    char text[112];
    snprintf(
      text,
      sizeof(text),
      "%s %s %s %+d  %s",
      activeCryptos[i].symbol,
      priceText,
      signal.action,
      signal.score,
      signal.reason
    );

    lv_label_set_text(label, text);

    uint32_t color = 0xCCCCCC;
    if (strcmp(signal.action, "BUY") == 0) color = 0x00FF00;
    else if (strcmp(signal.action, "SELL") == 0) color = 0xFF4040;
    else if (strcmp(signal.action, "WATCH") == 0) color = 0xF4B400;
    else if (strcmp(signal.action, "WAIT") == 0) color = 0xAAAAAA;

    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  }

  tradingSignalsDirty = false;
}

#else

void tradingSignalsRender() {}

#endif
