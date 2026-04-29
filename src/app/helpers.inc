// ---------------- Helpers ----------------
void set_status(const char *msg) {
  if (status_label) lv_label_set_text(status_label, msg);
}

bool batteryMonitorEnabled() {
#if ENABLE_BATTERY_MONITOR
  return BATTERY_ADC_PIN >= 0;
#else
  return false;
#endif
}

int readBatteryRaw() {
  if (!batteryMonitorEnabled()) return -1;

  int total = 0;
  for (int i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    total += analogRead(BATTERY_ADC_PIN);
  }
  return total / BATTERY_SAMPLE_COUNT;
}

int readBatteryAdcMilliVolts() {
  if (!batteryMonitorEnabled()) return -1;

  uint32_t total = 0;
  for (int i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    total += analogReadMilliVolts(BATTERY_ADC_PIN);
  }
  return (int)(total / BATTERY_SAMPLE_COUNT);
}

float batteryVoltageFromAdcMilliVolts(int adcMv) {
  if (adcMv < 0) return NAN;
  return ((float)adcMv / 1000.0f) * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION;
}

int batteryPercentFromVoltage(float voltage) {
  if (isnan(voltage)) return -1;

  if (voltage <= BATTERY_MIN_V) return 0;
  if (voltage >= BATTERY_MAX_V) return 100;

  // Approximate LiPo open-circuit discharge curve using piecewise linear segments.
  // This is much more realistic than a straight 3.2V-4.2V mapping.
  static const float voltageTable[] = {
    3.20f, 3.27f, 3.61f, 3.69f, 3.71f,
    3.73f, 3.75f, 3.77f, 3.79f, 3.81f,
    3.83f, 3.85f, 3.87f, 3.92f, 3.97f,
    4.02f, 4.08f, 4.11f, 4.15f, 4.20f
  };
  static const int percentTable[] = {
     0,  5, 10, 15, 20,
    25, 30, 35, 40, 45,
    50, 55, 60, 65, 70,
    75, 80, 85, 90, 100
  };

  const int pointCount = (int)(sizeof(percentTable) / sizeof(percentTable[0]));
  for (int i = 1; i < pointCount; i++) {
    if (voltage <= voltageTable[i]) {
      float v0 = voltageTable[i - 1];
      float v1 = voltageTable[i];
      int p0 = percentTable[i - 1];
      int p1 = percentTable[i];
      float fraction = (voltage - v0) / (v1 - v0);
      return (int)lroundf(p0 + fraction * (float)(p1 - p0));
    }
  }

  return 100;
}

void updateBatteryStatus(bool force) {
  if (!battery_label) return;

  if (!batteryMonitorEnabled()) {
    lv_label_set_text(battery_label, "Batt off");
    return;
  }

  unsigned long now = millis();
  if (!force && now - lastBatteryReadMs < BATTERY_UPDATE_MS) return;
  lastBatteryReadMs = now;

  lastBatteryRaw = readBatteryRaw();
  lastBatteryAdcMv = readBatteryAdcMilliVolts();
  lastBatteryVoltage = batteryVoltageFromAdcMilliVolts(lastBatteryAdcMv);
  lastBatteryPercent = batteryPercentFromVoltage(lastBatteryVoltage);

  char text[24];
  if (lastBatteryRaw < 0 || lastBatteryAdcMv < 0 || isnan(lastBatteryVoltage) || lastBatteryPercent < 0) {
    snprintf(text, sizeof(text), "Batt --");
  } else {
    snprintf(text, sizeof(text), "Batt %.2fV %d%%", lastBatteryVoltage, lastBatteryPercent);
  }
  lv_label_set_text(battery_label, text);

  if (memory_label) {
    String memoryText = "Mem ";
    memoryText += formatMemoryText();
    lv_label_set_text(memory_label, memoryText.c_str());
  }

}

int clampScreenBrightnessPercent(int value) {
  if (value < 5) return 5;
  if (value > 100) return 100;
  return value;
}

int clampIntRange(int value, int minValue, int maxValue) {
  if (value < minValue) return minValue;
  if (value > maxValue) return maxValue;
  return value;
}

unsigned long secondsToMs(int seconds) {
  return (unsigned long)seconds * 1000UL;
}

unsigned long minutesToMs(int minutes) {
  return (unsigned long)minutes * 60UL * 1000UL;
}

unsigned long hoursToMs(int hours) {
  return (unsigned long)hours * 60UL * 60UL * 1000UL;
}

unsigned long cryptoPriceRefreshIntervalMs() {
  return secondsToMs(deviceConfig.cgCurrentRefreshSeconds);
}

unsigned long cryptoCurrentRetryIntervalMs() {
  return minutesToMs(deviceConfig.cgCurrentRetryMinutes);
}

bool cryptoCurrentBackoffActive() {
  return nextCryptoCurrentRetryMs != 0 && (long)(millis() - nextCryptoCurrentRetryMs) < 0;
}

unsigned long cryptoBackgroundRefreshIntervalMs() {
  return minutesToMs(deviceConfig.cgBackgroundRefreshMinutes);
}

unsigned long cryptoWebRefreshIntervalMs() {
  return secondsToMs(deviceConfig.cgWebRefreshSeconds);
}

unsigned long cryptoHistoryStepIntervalMs() {
  return secondsToMs(deviceConfig.cgHistoryStepSeconds);
}

unsigned long cryptoHistoryRetryIntervalMs() {
  return minutesToMs(deviceConfig.cgHistoryRetryMinutes);
}

unsigned long historyRefreshIntervalMs() {
  return hoursToMs(deviceConfig.cgHistoryRefreshHours);
}

void initBacklightControl() {
  ledcSetup(TFT_BL_PWM_CHANNEL, TFT_BL_PWM_FREQ, TFT_BL_PWM_RESOLUTION);
  ledcAttachPin(TFT_BL_PIN, TFT_BL_PWM_CHANNEL);
  int duty = (DEFAULT_SCREEN_BRIGHTNESS_PERCENT * TFT_BL_PWM_MAX_DUTY) / 100;
  ledcWrite(TFT_BL_PWM_CHANNEL, duty);
}

void applyScreenBrightness() {
  deviceConfig.screenBrightnessPercent = clampScreenBrightnessPercent(deviceConfig.screenBrightnessPercent);
  int duty = (deviceConfig.screenBrightnessPercent * TFT_BL_PWM_MAX_DUTY) / 100;
  ledcAttachPin(TFT_BL_PIN, TFT_BL_PWM_CHANNEL);
  ledcWrite(TFT_BL_PWM_CHANNEL, duty);
  Serial.printf("Display: brightness=%d%% duty=%d\n", deviceConfig.screenBrightnessPercent, duty);
}

bool containsIgnoreCase(const char *haystack, const char *needle) {
  if (!haystack || !needle || !*needle) return false;

  for (const char *h = haystack; *h; ++h) {
    const char *a = h;
    const char *b = needle;
    while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
      ++a;
      ++b;
    }
    if (*b == '\0') return true;
  }

  return false;
}

int cryptoVisibleRowCount() {
  return (configuredCryptoCount < CRYPTO_VISIBLE_ROWS) ? configuredCryptoCount : CRYPTO_VISIBLE_ROWS;
}

bool cryptoAutoScrollEnabled() {
  return configuredCryptoCount > CRYPTO_VISIBLE_ROWS;
}

int cryptoMaxScrollOffset() {
  int maxOffset = configuredCryptoCount - CRYPTO_VISIBLE_ROWS;
  return (maxOffset > 0) ? maxOffset : 0;
}

int cryptoActiveIndexForSlot(int slot) {
  int activeIndex = cryptoScrollOffset + slot;
  return (activeIndex >= 0 && activeIndex < configuredCryptoCount) ? activeIndex : -1;
}

void formatCryptoPriceText(float value, int decimals, char *out, size_t outSize) {
  if (!out || outSize == 0) return;

  if (isnan(value)) {
    snprintf(out, outSize, "N/A");
    return;
  }

  char priceBuf[24];
  dtostrf(value, 0, decimals, priceBuf);
  char *trimmed = priceBuf;
  while (*trimmed == ' ') ++trimmed;

  snprintf(out, outSize, "$%s", trimmed);
}

void copyText(char *dest, size_t destSize, const char *src, const char *fallback = "") {
  const char *text = (src && *src) ? src : fallback;
  snprintf(dest, destSize, "%s", text);
}

bool isSafeHostnameChar(char c) {
  return isalnum((unsigned char)c) || c == '-';
}

void trimLine(char *text) {
  if (!text) return;

  char *start = text;
  while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
  if (start != text) memmove(text, start, strlen(start) + 1);

  size_t len = strlen(text);
  while (len > 0) {
    char c = text[len - 1];
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
    text[len - 1] = '\0';
    len--;
  }
}

void uppercaseText(char *text) {
  if (!text) return;
  for (size_t i = 0; text[i]; i++) {
    text[i] = (char)toupper((unsigned char)text[i]);
  }
}

void lowercaseText(char *text) {
  if (!text) return;
  for (size_t i = 0; text[i]; i++) {
    text[i] = (char)tolower((unsigned char)text[i]);
  }
}

void sanitizeHostname(char *text) {
  if (!text) return;

  int out = 0;
  for (int i = 0; text[i] && out < 31; i++) {
    char c = (char)tolower((unsigned char)text[i]);
    if (isSafeHostnameChar(c)) {
      text[out++] = c;
    }
  }
  text[out] = '\0';
}

String urlEncode(const char *text) {
  String encoded;
  if (!text) return encoded;

  const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; text[i]; i++) {
    unsigned char c = (unsigned char)text[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += (char)c;
    } else {
      encoded += '%';
      encoded += hex[(c >> 4) & 0x0F];
      encoded += hex[c & 0x0F];
    }
  }

  return encoded;
}

const char* timezoneToPosix(const char *timezoneName) {
  if (!timezoneName || !*timezoneName) return "EST5EDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/New_York") == 0) return "EST5EDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/Detroit") == 0) return "EST5EDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/Chicago") == 0) return "CST6CDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/Denver") == 0) return "MST7MDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/Phoenix") == 0) return "MST7";
  if (strcmp(timezoneName, "America/Los_Angeles") == 0) return "PST8PDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "America/Anchorage") == 0) return "AKST9AKDT,M3.2.0,M11.1.0";
  if (strcmp(timezoneName, "Pacific/Honolulu") == 0) return "HST10";
  return timezoneName;
}

String buildSecretsFileFromConfig() {
  String content;
  content.reserve(512);
  content += "wifi_ssid=";
  content += deviceConfig.wifiSsid;
  content += "\n";
  content += "wifi_password=";
  content += deviceConfig.wifiPassword;
  content += "\n";
  content += "web_password=";
  content += deviceConfig.webPassword;
  content += "\n";
  content += "owm_api_key=";
  content += deviceConfig.owmApiKey;
  content += "\n";
  content += "weather_location=";
  content += deviceConfig.weatherLocation;
  content += "\n";
  content += "timezone=";
  content += deviceConfig.timezone;
  content += "\n";
  content += "mdns_hostname=";
  content += deviceConfig.mdnsHostname;
  content += "\n";
  content += "screen_brightness=";
  content += String(clampScreenBrightnessPercent(deviceConfig.screenBrightnessPercent));
  content += "\n";
  content += "cg_current_refresh_seconds=";
  content += String(deviceConfig.cgCurrentRefreshSeconds);
  content += "\n";
  content += "cg_current_retry_minutes=";
  content += String(deviceConfig.cgCurrentRetryMinutes);
  content += "\n";
  content += "cg_background_refresh_minutes=";
  content += String(deviceConfig.cgBackgroundRefreshMinutes);
  content += "\n";
  content += "cg_web_refresh_seconds=";
  content += String(deviceConfig.cgWebRefreshSeconds);
  content += "\n";
  content += "cg_history_step_seconds=";
  content += String(deviceConfig.cgHistoryStepSeconds);
  content += "\n";
  content += "cg_history_retry_minutes=";
  content += String(deviceConfig.cgHistoryRetryMinutes);
  content += "\n";
  content += "cg_history_refresh_hours=";
  content += String(deviceConfig.cgHistoryRefreshHours);
  content += "\n";
  return content;
}

void applyTimezoneConfig() {
  const char *posixTz = timezoneToPosix(deviceConfig.timezone);
  configTzTime(posixTz, "pool.ntp.org", "time.nist.gov");
  Serial.printf("Time: timezone=%s posix=%s\n", deviceConfig.timezone, posixTz);
}

void resetDeviceConfigDefaults() {
  copyText(deviceConfig.wifiSsid, sizeof(deviceConfig.wifiSsid), DEFAULT_WIFI_SSID);
  copyText(deviceConfig.wifiPassword, sizeof(deviceConfig.wifiPassword), DEFAULT_WIFI_PASS);
  copyText(deviceConfig.webPassword, sizeof(deviceConfig.webPassword), DEFAULT_WEB_PASSWORD);
  copyText(deviceConfig.owmApiKey, sizeof(deviceConfig.owmApiKey), DEFAULT_OWM_API_KEY);
  copyText(deviceConfig.weatherLocation, sizeof(deviceConfig.weatherLocation), DEFAULT_OWM_LOCATION);
  copyText(deviceConfig.timezone, sizeof(deviceConfig.timezone), DEFAULT_TIMEZONE);
  copyText(deviceConfig.mdnsHostname, sizeof(deviceConfig.mdnsHostname), DEFAULT_MDNS_HOSTNAME);
  deviceConfig.screenBrightnessPercent = DEFAULT_SCREEN_BRIGHTNESS_PERCENT;
  deviceConfig.cgCurrentRefreshSeconds = DEFAULT_CG_CURRENT_REFRESH_SECONDS;
  deviceConfig.cgCurrentRetryMinutes = DEFAULT_CG_CURRENT_RETRY_MINUTES;
  deviceConfig.cgBackgroundRefreshMinutes = DEFAULT_CG_BACKGROUND_REFRESH_MINUTES;
  deviceConfig.cgWebRefreshSeconds = DEFAULT_CG_WEB_REFRESH_SECONDS;
  deviceConfig.cgHistoryStepSeconds = DEFAULT_CG_HISTORY_STEP_SECONDS;
  deviceConfig.cgHistoryRetryMinutes = DEFAULT_CG_HISTORY_RETRY_MINUTES;
  deviceConfig.cgHistoryRefreshHours = DEFAULT_CG_HISTORY_REFRESH_HOURS;

  deviceConfigStatus.sdCardAvailable = false;
  deviceConfigStatus.secretsFileFound = false;
  deviceConfigStatus.loadedFromSd = false;
  deviceConfigStatus.wifiFromSd = false;
  deviceConfigStatus.weatherKeyFromSd = false;
  deviceConfigStatus.weatherLocationFromSd = false;
  deviceConfigStatus.timezoneFromSd = false;
  deviceConfigStatus.mdnsHostnameFromSd = false;
  deviceConfigStatus.screenBrightnessFromSd = false;
  deviceConfigStatus.coinGeckoTimingFromSd = false;
}

void applyDeviceConfigValue(const char *key, const char *value) {
  if (!key || !value) return;

  if (strcmp(key, "wifi_ssid") == 0) {
    copyText(deviceConfig.wifiSsid, sizeof(deviceConfig.wifiSsid), value);
    deviceConfigStatus.wifiFromSd = true;
  } else if (strcmp(key, "wifi_password") == 0) {
    copyText(deviceConfig.wifiPassword, sizeof(deviceConfig.wifiPassword), value);
    deviceConfigStatus.wifiFromSd = true;
  } else if (strcmp(key, "web_password") == 0) {
    copyText(deviceConfig.webPassword, sizeof(deviceConfig.webPassword), value);
  } else if (strcmp(key, "owm_api_key") == 0) {
    copyText(deviceConfig.owmApiKey, sizeof(deviceConfig.owmApiKey), value);
    deviceConfigStatus.weatherKeyFromSd = true;
  } else if (strcmp(key, "weather_location") == 0) {
    copyText(deviceConfig.weatherLocation, sizeof(deviceConfig.weatherLocation), value);
    deviceConfigStatus.weatherLocationFromSd = true;
  } else if (strcmp(key, "timezone") == 0) {
    copyText(deviceConfig.timezone, sizeof(deviceConfig.timezone), value);
    deviceConfigStatus.timezoneFromSd = true;
  } else if (strcmp(key, "mdns_hostname") == 0) {
    copyText(deviceConfig.mdnsHostname, sizeof(deviceConfig.mdnsHostname), value);
    deviceConfigStatus.mdnsHostnameFromSd = true;
  } else if (strcmp(key, "screen_brightness") == 0 || strcmp(key, "screen_brightness_percent") == 0) {
    deviceConfig.screenBrightnessPercent = clampScreenBrightnessPercent(atoi(value));
    deviceConfigStatus.screenBrightnessFromSd = true;
  } else if (strcmp(key, "cg_current_refresh_seconds") == 0) {
    deviceConfig.cgCurrentRefreshSeconds = clampIntRange(atoi(value), 30, 3600);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_current_retry_minutes") == 0) {
    deviceConfig.cgCurrentRetryMinutes = clampIntRange(atoi(value), 1, 1440);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_background_refresh_minutes") == 0) {
    deviceConfig.cgBackgroundRefreshMinutes = clampIntRange(atoi(value), 1, 1440);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_web_refresh_seconds") == 0) {
    deviceConfig.cgWebRefreshSeconds = clampIntRange(atoi(value), 30, 3600);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_history_step_seconds") == 0) {
    deviceConfig.cgHistoryStepSeconds = clampIntRange(atoi(value), 10, 3600);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_history_retry_minutes") == 0) {
    deviceConfig.cgHistoryRetryMinutes = clampIntRange(atoi(value), 1, 1440);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  } else if (strcmp(key, "cg_history_refresh_hours") == 0) {
    deviceConfig.cgHistoryRefreshHours = clampIntRange(atoi(value), 1, 168);
    deviceConfigStatus.coinGeckoTimingFromSd = true;
  }
}

String upsertConfigLine(const String& content, const char *targetKey, const String& targetValue) {
  String updated;
  updated.reserve(content.length() + targetValue.length() + 32);

  bool found = false;
  int start = 0;
  while (start < (int)content.length()) {
    int end = content.indexOf('\n', start);
    if (end < 0) end = content.length();

    String line = content.substring(start, end);
    String comparable = line;
    comparable.trim();

    int equals = comparable.indexOf('=');
    if (equals > 0) {
      String key = comparable.substring(0, equals);
      key.trim();
      key.toLowerCase();
      if (key == targetKey || (strcmp(targetKey, "screen_brightness") == 0 && key == "screen_brightness_percent")) {
        if (!found) {
          updated += targetKey;
          updated += "=";
          updated += targetValue;
          updated += "\n";
        }
        found = true;
      } else {
        updated += line;
        updated += "\n";
      }
    } else {
      updated += line;
      updated += "\n";
    }

    start = end + 1;
  }

  if (!found) {
    updated += targetKey;
    updated += "=";
    updated += targetValue;
    updated += "\n";
  }

  return updated;
}

bool ensureSdCardReady() {
  if (sdCardReady) return true;

  sdSpi.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
  sdCardReady = SD.begin(SD_CS_PIN, sdSpi);
  return sdCardReady;
}

String htmlEscape(const String& input) {
  String out;
  out.reserve(input.length() + 32);

  for (size_t i = 0; i < input.length(); i++) {
    char c = input[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += c; break;
    }
  }

  return out;
}

String loadTextFileFromSD(const char *path, bool &ok) {
  ok = false;
  if (!ensureSdCardReady()) return "";

  File file = SD.open(path, FILE_READ);
  if (!file) return "";

  String content;
  while (file.available()) {
    content += (char)file.read();
  }

  file.close();
  ok = true;
  return content;
}

bool saveTextFileToSD(const char *path, const String& content) {
  if (!ensureSdCardReady()) return false;

  if (SD.exists(path) && !SD.remove(path)) {
    return false;
  }

  File file = SD.open(path, FILE_WRITE);
  if (!file) return false;

  size_t written = file.print(content);
  file.close();
  return written == content.length();
}

void loadDeviceConfigurationFromSD() {
  resetDeviceConfigDefaults();

  if (!ensureSdCardReady()) {
    Serial.println("Secrets: SD card unavailable, using compiled defaults");
    return;
  }

  deviceConfigStatus.sdCardAvailable = true;

  File file = SD.open(DEVICE_SECRETS_PATH, FILE_READ);
  if (!file) {
    Serial.println("Secrets: secrets.txt missing, using compiled defaults");
    return;
  }

  deviceConfigStatus.secretsFileFound = true;
  deviceConfigStatus.loadedFromSd = true;

  while (file.available()) {
    String rawLine = file.readStringUntil('\n');
    char line[192];
    rawLine.toCharArray(line, sizeof(line));
    trimLine(line);

    if (line[0] == '\0' || line[0] == '#') continue;

    char *equals = strchr(line, '=');
    if (!equals) continue;

    *equals = '\0';
    char *key = line;
    char *value = equals + 1;
    trimLine(key);
    trimLine(value);
    lowercaseText(key);
    applyDeviceConfigValue(key, value);
  }

  file.close();

  sanitizeHostname(deviceConfig.mdnsHostname);
  if (deviceConfig.mdnsHostname[0] == '\0') {
    copyText(deviceConfig.mdnsHostname, sizeof(deviceConfig.mdnsHostname), DEFAULT_MDNS_HOSTNAME);
  }

  Serial.printf(
    "Secrets: source=%s wifi=%s weather_key=%s location=%s timezone=%s host=%s\n",
    deviceConfigStatus.loadedFromSd ? "sd" : "defaults",
    deviceConfigStatus.wifiFromSd ? "sd" : "defaults",
    deviceConfigStatus.weatherKeyFromSd ? "sd" : "defaults",
    deviceConfigStatus.weatherLocationFromSd ? "sd" : "defaults",
    deviceConfigStatus.timezoneFromSd ? "sd" : "defaults",
    deviceConfigStatus.mdnsHostnameFromSd ? "sd" : "defaults"
  );
  Serial.printf(
    "Secrets: loaded ssid=%s host=%s weather_location=%s timezone=%s brightness=%d%% cg_current=%ds cg_current_retry=%dm cg_bg=%dm cg_hist_step=%ds\n",
    deviceConfig.wifiSsid[0] ? deviceConfig.wifiSsid : "(empty)",
    deviceConfig.mdnsHostname,
    deviceConfig.weatherLocation,
    deviceConfig.timezone,
    deviceConfig.screenBrightnessPercent,
    deviceConfig.cgCurrentRefreshSeconds,
    deviceConfig.cgCurrentRetryMinutes,
    deviceConfig.cgBackgroundRefreshMinutes,
    deviceConfig.cgHistoryStepSeconds
  );
}
