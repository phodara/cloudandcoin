bool fetchCurrentPricesUsd(float outVals[], int count) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("CG current batch: skipped, WiFi disconnected");
    for (int i = 0; i < count; i++) outVals[i] = NAN;
    return false;
  }

  bool anyOk = false;
  for (int i = 0; i < count; i++) outVals[i] = NAN;

  const int batchSize = 4;
  for (int batchStart = 0; batchStart < count; batchStart += batchSize) {
    int batchCount = count - batchStart;
    if (batchCount > batchSize) batchCount = batchSize;

    char ids[160] = "";
    for (int i = 0; i < batchCount; i++) {
      if (i > 0) strncat(ids, ",", sizeof(ids) - strlen(ids) - 1);
      strncat(ids, activeCryptos[batchStart + i].coinGeckoId, sizeof(ids) - strlen(ids) - 1);
    }

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    char url[320];
    snprintf(
      url,
      sizeof(url),
      "https://api.coingecko.com/api/v3/simple/price?ids=%s&vs_currencies=usd",
      ids
    );

    if (!http.begin(url)) {
      Serial.printf("CG current batch: ids=%s begin failed\n", ids);
      continue;
    }

    int code = http.GET();
    if (code != 200) {
      Serial.printf("CG current batch: ids=%s HTTP %d\n", ids, code);
      http.end();
      continue;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.printf("CG current batch: ids=%s JSON parse failed: %s\n", ids, err.c_str());
      continue;
    }

    for (int i = 0; i < batchCount; i++) {
      int activeIndex = batchStart + i;
      JsonVariant usd = doc[activeCryptos[activeIndex].coinGeckoId]["usd"];
      if (usd.isNull()) {
        Serial.printf("CG current batch: %s missing usd field\n", activeCryptos[activeIndex].coinGeckoId);
        continue;
      }

      outVals[activeIndex] = usd.as<float>();
      anyOk = true;
      Serial.printf("CG current batch: %s = %.6f\n", activeCryptos[activeIndex].coinGeckoId, outVals[activeIndex]);
    }
  }

  return anyOk;
}

void loadCryptoConfigurationFromSD() {
  resetCryptoSelectionDefaults();

  if (!ensureSdCardReady()) {
    Serial.println("SD: card not available, using default tickers");
    return;
  }

  File file = SD.open(CRYPTO_TICKERS_PATH, FILE_READ);
  if (!file) {
    Serial.println("SD: crypto_tickers.txt missing, using default tickers");
    return;
  }

  int selectedCount = 0;

  while (file.available() && selectedCount < MAX_ACTIVE_CRYPTO_COUNT) {
    String rawLine = file.readStringUntil('\n');
    char line[160];
    rawLine.toCharArray(line, sizeof(line));
    trimLine(line);

    if (line[0] == '\0' || line[0] == '#') continue;

    char *parts[4] = {nullptr, nullptr, nullptr, nullptr};
    int partCount = 0;
    char *cursor = line;
    parts[partCount++] = cursor;
    while (*cursor && partCount < 4) {
      if (*cursor == '|') {
        *cursor = '\0';
        parts[partCount++] = cursor + 1;
      }
      cursor++;
    }

    for (int i = 0; i < partCount; i++) {
      trimLine(parts[i]);
    }

    uppercaseText(parts[0]);

    if (partCount == 1) {
      const CryptoDefinition* match = findCryptoDefinition(parts[0]);
      if (!match) {
        Serial.printf("SD: unsupported legacy ticker %s\n", parts[0]);
        continue;
      }

      bool duplicate = false;
      for (int i = 0; i < selectedCount; i++) {
        if (strcmp(activeCryptos[i].symbol, match->symbol) == 0) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) continue;

      setActiveCryptoFromDefinition(selectedCount, match);
      Serial.printf("SD: loaded legacy ticker %s\n", match->symbol);
      selectedCount++;
      continue;
    }

    if (partCount < 3) {
      Serial.printf("SD: invalid config line for %s\n", parts[0]);
      continue;
    }

    const char *coinGeckoId = nullptr;
    int decimals = 0;

    if (partCount >= 4) {
      coinGeckoId = parts[2];
      decimals = atoi(parts[3]);
    } else {
      coinGeckoId = parts[1];
      decimals = atoi(parts[2]);
    }

    bool duplicate = false;
    for (int i = 0; i < selectedCount; i++) {
      if (strcmp(activeCryptos[i].symbol, parts[0]) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    setActiveCryptoCustom(selectedCount, parts[0], coinGeckoId, decimals);
    Serial.printf("SD: loaded %s via metadata\n", activeCryptos[selectedCount].symbol);
    selectedCount++;
  }

  file.close();

  if (selectedCount == 0) {
    Serial.println("SD: no supported tickers found, using default tickers");
    return;
  }

  configuredCryptoCount = selectedCount;
  cryptoScrollOffset = 0;
  pairTradingDirty = true;
  tradingSignalsDirty = true;
}

// ---------------- CoinGecko history ----------------
bool fetchHistory30(const char* coinId, float outVals[], int &outCount) {
  outCount = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("CG history: %s skipped, WiFi disconnected\n", coinId);
    return false;
  }

  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(8000);
  char url[192];
  snprintf(
    url,
    sizeof(url),
    "https://api.coingecko.com/api/v3/coins/%s/market_chart?vs_currency=usd&days=30&interval=daily",
    coinId
  );

  http.begin(url);
  int code = http.GET();
  if (code != 200) {
    Serial.printf("CG history: %s HTTP %d\n", coinId, code);
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(16384);
  DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("CG history: %s JSON parse failed: %s\n", coinId, err.c_str());
    return false;
  }

  JsonArray prices = doc["prices"].as<JsonArray>();
  int n = prices.size();
  if (n <= 0) {
    Serial.printf("CG history: %s returned no prices\n", coinId);
    return false;
  }

  Serial.printf("CG history: %s returned %d points\n", coinId, n);

  if (n >= HISTORY_POINTS) {
    for (int i = 0; i < HISTORY_POINTS; i++) {
      int idx = (int)round((float)i * (n - 1) / (HISTORY_POINTS - 1));
      outVals[outCount++] = prices[idx][1].as<float>();
    }
  } else {
    for (int i = 0; i < n; i++) {
      outVals[outCount++] = prices[i][1].as<float>();
    }
    while (outCount < HISTORY_POINTS && outCount > 0) {
      outVals[outCount] = outVals[outCount - 1];
      outCount++;
    }
  }

  bool ok = outCount == HISTORY_POINTS;
  if (!ok) {
    Serial.printf("CG history: %s produced only %d output points\n", coinId, outCount);
  }
  return ok;
}
