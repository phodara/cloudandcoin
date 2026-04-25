// ---------------- OpenWeather 4-day forecast ----------------
bool fetchForecast4() {
  forecast_ok = false;
  todayHiLoOk = false;

  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String encodedLocation = urlEncode(deviceConfig.weatherLocation);
  String url = "https://api.openweathermap.org/data/2.5/forecast?q=";
  url += encodedLocation;
  url += "&units=imperial&appid=";
  url += deviceConfig.owmApiKey;

  if (!http.begin(url)) return false;

  int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  DynamicJsonDocument filter(2048);
  filter["list"][0]["dt"] = true;
  filter["list"][0]["main"]["temp_min"] = true;
  filter["list"][0]["main"]["temp_max"] = true;
  filter["list"][0]["weather"][0]["main"] = true;
  filter["city"]["timezone"] = true;

  DynamicJsonDocument doc(24576);
  DeserializationError err = deserializeJson(
    doc,
    http.getStream(),
    DeserializationOption::Filter(filter)
  );
  http.end();
  if (err) return false;

  JsonArray list = doc["list"].as<JsonArray>();
  if (list.isNull() || list.size() == 0) return false;

  long forecastTzOffset = doc["city"]["timezone"] | 0;
  time_t now = time(nullptr);
  if (now < 100000) return false;

  time_t localNow = now + forecastTzOffset;
  struct tm tmNow;
  gmtime_r(&localNow, &tmNow);

  char todayBuf[11];
  snprintf(todayBuf, sizeof(todayBuf), "%04d-%02d-%02d",
           tmNow.tm_year + 1900, tmNow.tm_mon + 1, tmNow.tm_mday);

  float todayHighF = NAN;
  float todayLowF = NAN;
  bool todayInit = false;

  char pickedDates[4][11] = {};
  float dayHigh[4];
  float dayLow[4];
  char dayCond[4][16] = {};
  int dayWeekday[4] = {0, 0, 0, 0};
  bool init[4] = {false, false, false, false};

  int found = 0;

  for (JsonObject item : list) {
    char datePart[11];
    long itemDt = item["dt"] | 0;
    if (itemDt <= 0) continue;

    time_t itemLocal = (time_t)itemDt + forecastTzOffset;
    struct tm itemTm;
    gmtime_r(&itemLocal, &itemTm);
    snprintf(
      datePart,
      sizeof(datePart),
      "%04d-%02d-%02d",
      itemTm.tm_year + 1900,
      itemTm.tm_mon + 1,
      itemTm.tm_mday
    );

    float tMax = item["main"]["temp_max"] | NAN;
    float tMin = item["main"]["temp_min"] | NAN;
    const char *cond = item["weather"][0]["main"] | "N/A";

    if (strcmp(datePart, todayBuf) == 0) {
      if (!todayInit) {
        todayHighF = tMax;
        todayLowF = tMin;
        todayInit = true;
      } else {
        if (!isnan(tMax) && (isnan(todayHighF) || tMax > todayHighF)) todayHighF = tMax;
        if (!isnan(tMin) && (isnan(todayLowF) || tMin < todayLowF)) todayLowF = tMin;
      }
      continue;
    }

    int slot = -1;
    for (int i = 0; i < found; i++) {
      if (strcmp(pickedDates[i], datePart) == 0) {
        slot = i;
        break;
      }
    }

    if (slot == -1) {
      if (found >= 4) continue;
      slot = found;
      copyText(pickedDates[slot], sizeof(pickedDates[slot]), datePart);
      found++;
    }

    if (!init[slot]) {
      dayHigh[slot] = tMax;
      dayLow[slot] = tMin;
      copyText(dayCond[slot], sizeof(dayCond[slot]), cond, "N/A");
      dayWeekday[slot] = itemTm.tm_wday;
      init[slot] = true;
    } else {
      if (!isnan(tMax) && (isnan(dayHigh[slot]) || tMax > dayHigh[slot])) dayHigh[slot] = tMax;
      if (!isnan(tMin) && (isnan(dayLow[slot]) || tMin < dayLow[slot])) dayLow[slot] = tMin;
    }
  }

  if (found < 4) return false;

  if (todayInit) {
    todayHigh = isnan(todayHighF) ? 0 : (int)round(todayHighF);
    todayLow  = isnan(todayLowF) ? 0 : (int)round(todayLowF);
    todayHiLoOk = true;
  } else {
    todayHigh = isnan(dayHigh[0]) ? 0 : (int)round(dayHigh[0]);
    todayLow  = isnan(dayLow[0])  ? 0 : (int)round(dayLow[0]);
    todayHiLoOk = true;
  }

  for (int i = 0; i < 4; i++) {
    copyText(forecast[i].day, sizeof(forecast[i].day), shortDayName(dayWeekday[i]), "---");
    copyText(forecast[i].cond, sizeof(forecast[i].cond), dayCond[i], "N/A");
    forecast[i].high = isnan(dayHigh[i]) ? 0 : (int)round(dayHigh[i]);
    forecast[i].low  = isnan(dayLow[i])  ? 0 : (int)round(dayLow[i]);
  }

  forecast_ok = true;
  return true;
}

bool fetchCurrentWeather(float &tempOut, float &pressureOut, char *condOut, size_t condOutSize) {
  tempOut = NAN;
  pressureOut = NAN;
  copyText(condOut, condOutSize, "unknown");

  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  String encodedLocation = urlEncode(deviceConfig.weatherLocation);
  String url = "https://api.openweathermap.org/data/2.5/weather?q=";
  url += encodedLocation;
  url += "&units=imperial&appid=";
  url += deviceConfig.owmApiKey;

  if (!http.begin(url)) return false;

  int code = http.GET();
  if (code != 200) {
    Serial.printf("OWM current: HTTP %d\n", code);
    http.end();
    return false;
  }

  DynamicJsonDocument filter(512);
  filter["main"]["temp"] = true;
  filter["main"]["pressure"] = true;
  filter["weather"][0]["main"] = true;

  DynamicJsonDocument doc(2048);
  DeserializationError err = deserializeJson(
    doc,
    http.getStream(),
    DeserializationOption::Filter(filter)
  );
  http.end();
  if (err) {
    Serial.printf("OWM current: JSON parse failed: %s\n", err.c_str());
    return false;
  }

  tempOut = doc["main"]["temp"] | NAN;
  pressureOut = doc["main"]["pressure"] | NAN;
  const char *cond = doc["weather"][0]["main"] | "unknown";
  copyText(condOut, condOutSize, cond, "unknown");
  return true;
}

