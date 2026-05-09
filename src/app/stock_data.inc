void resetStockSelectionDefaults() {
  static const char *defaults[] = {"AAPL", "MSFT", "NVDA", "SPY"};
  const int defaultCount = sizeof(defaults) / sizeof(defaults[0]);
  configuredStockCount = defaultCount < STOCK_VISIBLE_ROWS ? defaultCount : STOCK_VISIBLE_ROWS;
  stockScrollOffset = 0;

  for (int i = 0; i < MAX_ACTIVE_STOCK_COUNT; i++) {
    const char *symbol = defaults[i % defaultCount];
    snprintf(activeStocks[i].symbol, sizeof(activeStocks[i].symbol), "%s", symbol);
    currentStockValues[i] = NAN;
    previousStockValues[i] = NAN;
    currentStockChange[i] = NAN;
    currentStockChangePercent[i] = NAN;
  }
}

void setActiveStockSymbol(int index, const char *symbol) {
  if (index < 0 || index >= MAX_ACTIVE_STOCK_COUNT || !symbol || !*symbol) return;
  snprintf(activeStocks[index].symbol, sizeof(activeStocks[index].symbol), "%s", symbol);
  currentStockValues[index] = NAN;
  previousStockValues[index] = NAN;
  currentStockChange[index] = NAN;
  currentStockChangePercent[index] = NAN;
}

void loadStockConfigurationFromSD() {
  resetStockSelectionDefaults();

  if (!ensureSdCardReady()) {
    Serial.println("SD: card not available, using default stocks");
    return;
  }

  File file = SD.open(STOCK_TICKERS_PATH, FILE_READ);
  if (!file) {
    Serial.println("SD: stock_tickers.txt missing, using default stocks");
    return;
  }

  int selectedCount = 0;

  while (file.available() && selectedCount < MAX_ACTIVE_STOCK_COUNT) {
    String rawLine = file.readStringUntil('\n');
    char line[80];
    rawLine.toCharArray(line, sizeof(line));
    trimLine(line);

    if (line[0] == '\0' || line[0] == '#') continue;

    char *separator = strchr(line, '|');
    if (separator) *separator = '\0';
    trimLine(line);
    uppercaseText(line);

    if (line[0] == '\0') continue;

    bool duplicate = false;
    for (int i = 0; i < selectedCount; i++) {
      if (strcmp(activeStocks[i].symbol, line) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    setActiveStockSymbol(selectedCount, line);
    Serial.printf("SD: loaded stock %s\n", activeStocks[selectedCount].symbol);
    selectedCount++;
  }

  file.close();

  if (selectedCount == 0) {
    Serial.println("SD: no stock tickers found, using default stocks");
    return;
  }

  configuredStockCount = selectedCount;
  stockScrollOffset = 0;
}

bool fetchCurrentStockQuotes(float outVals[], float outChange[], float outChangePercent[], int count) {
  for (int i = 0; i < count; i++) {
    outVals[i] = NAN;
    outChange[i] = NAN;
    outChangePercent[i] = NAN;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Finnhub quotes: skipped, WiFi disconnected");
    return false;
  }

  if (deviceConfig.finnhubApiKey[0] == '\0') {
    Serial.println("Finnhub quotes: skipped, API key missing");
    return false;
  }

  bool anyOk = false;
  for (int i = 0; i < count; i++) {
    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);

    String url = "https://finnhub.io/api/v1/quote?symbol=";
    url += urlEncode(activeStocks[i].symbol);
    url += "&token=";
    url += urlEncode(deviceConfig.finnhubApiKey);

    if (!http.begin(url)) {
      Serial.printf("Finnhub quotes: %s begin failed\n", activeStocks[i].symbol);
      continue;
    }

    int code = http.GET();
    if (code != 200) {
      Serial.printf("Finnhub quotes: %s HTTP %d\n", activeStocks[i].symbol, code);
      http.end();
      continue;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(512);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("Finnhub quotes: %s JSON parse failed: %s\n", activeStocks[i].symbol, err.c_str());
      continue;
    }

    float current = doc["c"] | NAN;
    float change = doc["d"] | NAN;
    float changePercent = doc["dp"] | NAN;
    if (isnan(current) || current <= 0.0f) {
      Serial.printf("Finnhub quotes: %s missing current price\n", activeStocks[i].symbol);
      continue;
    }

    outVals[i] = current;
    outChange[i] = change;
    outChangePercent[i] = changePercent;
    anyOk = true;
    Serial.printf("Finnhub quotes: %s = %.2f\n", activeStocks[i].symbol, current);

    if (i < count - 1) delay(150);
  }

  return anyOk;
}
