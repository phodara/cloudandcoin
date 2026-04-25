const char* configSourceLabel(bool fromSd) {
  return fromSd ? "SD" : "Defaults";
}

bool webAuthRequired() {
  return deviceConfig.webPassword[0] != '\0';
}

bool ensureWebAuthorized() {
  if (!webAuthRequired()) return true;
  if (webServer.authenticate("admin", deviceConfig.webPassword)) return true;
  webServer.requestAuthentication(BASIC_AUTH, "Cloud and Coin", "Enter web password");
  return false;
}

String editorChromeStart(const char *title, const char *subtitle) {
  String body;
  body.reserve(6144);
  body += "<!doctype html><html><head><meta charset=\"utf-8\">";
  body += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  body += "<title>";
  body += htmlEscape(title);
  body += "</title><style>";
  body += "body{margin:0;background:#101418;color:#eef2f6;font-family:Helvetica,Arial,sans-serif;}";
  body += ".wrap{max-width:900px;margin:0 auto;padding:24px;}";
  body += ".card{background:#1b222a;border:1px solid #303946;border-radius:14px;padding:20px;}";
  body += "h1{margin:0;font-size:28px;}h2{margin:18px 0 8px;font-size:22px;color:#eef2f6;}";
  body += "p{color:#b8c3cf;line-height:1.5;}.brand-note{margin:6px 0 0;font-size:14px;color:#8fa0b2;}";
  body += ".msg{margin:16px 0;padding:12px 14px;border-radius:10px;font-weight:600;}";
  body += ".ok{background:#123524;color:#8ef0b0;border:1px solid #24583a;}";
  body += ".err{background:#3a1717;color:#ffb3b3;border:1px solid #6d2a2a;}";
  body += ".meta{margin:16px 0;padding:14px;background:#121820;border:1px solid #2c3540;border-radius:10px;}";
  body += ".meta p{margin:6px 0;color:#d8e4f0;}";
  body += ".section{margin:16px 0;padding:14px;background:#121820;border:1px solid #2c3540;border-radius:10px;}";
  body += ".section h2{margin:0 0 8px;font-size:18px;color:#eef2f6;}";
  body += ".section ul{margin:8px 0 0 20px;padding:0;color:#d8e4f0;}";
  body += ".section li{margin:6px 0;}";
  body += ".nav{display:flex;gap:10px;flex-wrap:wrap;margin:14px 0 8px;}";
  body += ".nav a{color:#9dd7ff;text-decoration:none;background:#121820;border:1px solid #2c3540;border-radius:999px;padding:8px 12px;}";
  body += "input{width:100%;margin-top:10px;background:#0d1117;color:#e6edf3;border:1px solid #39424e;border-radius:10px;padding:12px;font:15px/1.4 monospace;box-sizing:border-box;}";
  body += "label{display:block;margin-top:14px;color:#d8e4f0;font-weight:600;}";
  body += "textarea{width:100%;min-height:360px;margin-top:14px;background:#0d1117;color:#e6edf3;";
  body += "border:1px solid #39424e;border-radius:10px;padding:14px;font:15px/1.45 monospace;box-sizing:border-box;}";
  body += "button{margin-top:14px;background:#3aa675;color:#08120d;border:0;border-radius:10px;";
  body += "padding:12px 18px;font-size:16px;font-weight:700;cursor:pointer;}";
  body += ".button-gap{margin-left:12px;}";
  body += "code{background:#0d1117;padding:2px 6px;border-radius:6px;color:#d8e4f0;}";
  body += "</style></head><body><div class=\"wrap\"><div class=\"card\">";
  body += "<h1>Cloud and Coin</h1>";
  body += "<p class=\"brand-note\">Copyright 2026 Paul Hodara</p>";
  body += "<h2>";
  body += htmlEscape(title);
  body += "</h2><p>";
  body += htmlEscape(subtitle);
  body += "</p>";
  if (!setupModeActive) {
    body += "<div class=\"nav\">";
    body += "<a href=\"/view\">View</a><a href=\"/tickers\">Tickers</a><a href=\"/secrets\">Secrets</a><a href=\"/info\">Info</a>";
    body += "</div>";
  }
  return body;
}

String configSummaryHtml() {
  String body;
  body += "<div class=\"meta\">";
  body += "<p>Secrets file: ";
  body += deviceConfigStatus.secretsFileFound ? "Loaded from SD" : "Using compiled defaults";
  body += "</p><p>WiFi config: ";
  body += configSourceLabel(deviceConfigStatus.wifiFromSd);
  body += "</p><p>Weather API key: ";
  body += configSourceLabel(deviceConfigStatus.weatherKeyFromSd);
  body += "</p><p>Weather location: ";
  body += configSourceLabel(deviceConfigStatus.weatherLocationFromSd);
  body += "</p><p>mDNS host: <code>";
  body += htmlEscape(deviceConfig.mdnsHostname);
  body += ".local</code> (";
  body += configSourceLabel(deviceConfigStatus.mdnsHostnameFromSd);
  body += ")</p><p>Timezone: <code>";
  body += htmlEscape(deviceConfig.timezone);
  body += "</code> (";
  body += configSourceLabel(deviceConfigStatus.timezoneFromSd);
  body += ")</p></div>";
  return body;
}

void appendMessage(String& body, const char *message, bool success) {
  if (!message || !*message) return;
  body += "<div class=\"msg ";
  body += success ? "ok" : "err";
  body += "\">";
  body += htmlEscape(message);
  body += "</div>";
}

void sendTickerEditorPage(const char *message, bool success) {
  if (!ensureWebAuthorized()) return;

  bool fileOk = false;
  String fileContent = loadTextFileFromSD(CRYPTO_TICKERS_PATH, fileOk);

  String body = editorChromeStart("Ticker Editor", "Edit /crypto_tickers.txt over Wi-Fi and save directly to the SD card.");
  appendMessage(body, message, success);

  if (!fileOk) {
    body += "<div class=\"msg err\">SD card unavailable or file could not be opened.</div>";
    fileContent = "# Example\nBTC\nETH\nADA\n";
  }

  body += configSummaryHtml();
  body += "<form method=\"POST\" action=\"/tickers/save\">";
  body += "<textarea name=\"content\" spellcheck=\"false\">";
  body += htmlEscape(fileContent);
  body += "</textarea>";
  body += "<br><button type=\"submit\">Save To SD Card</button></form>";
  body += "</div></div></body></html>";

  webServer.send(200, "text/html", body);
}

void handleTickerEditorRoot() {
  sendTickerEditorPage("", true);
}

void handleTickerEditorSave() {
  if (!ensureWebAuthorized()) return;

  if (!webServer.hasArg("content")) {
    sendTickerEditorPage("No form data was received.", false);
    return;
  }

  String content = webServer.arg("content");
  if (!content.endsWith("\n")) content += "\n";

  if (!saveTextFileToSD(CRYPTO_TICKERS_PATH, content)) {
    sendTickerEditorPage("Save failed. Check the SD card and try again.", false);
    return;
  }

  loadCryptoConfigurationFromSD();
  set_status("Tickers saved");
  renderCryptoWindow();
  cryptoRefreshPending = true;
  startCryptoHistoryRefresh();
  sendTickerEditorPage("Saved. The ticker list reloaded. Prices and history will refresh in the background.", true);
}

void sendSecretsEditorPage(const char *message, bool success) {
  if (!ensureWebAuthorized()) return;

  bool fileOk = false;
  String fileContent = loadTextFileFromSD(DEVICE_SECRETS_PATH, fileOk);
  String body = editorChromeStart("Secrets Editor", "Edit /secrets.txt over Wi-Fi. Wi-Fi and hostname changes may require a reboot to fully apply.");
  appendMessage(body, message, success);

  if (!fileOk) {
    body += "<div class=\"msg err\">secrets.txt could not be opened. Saving will create it.</div>";
    fileContent = "";
  }

  body += configSummaryHtml();
  body += "<form method=\"POST\" action=\"/secrets/save\">";
  body += "<textarea name=\"content\" spellcheck=\"false\">";
  body += htmlEscape(fileContent);
  body += "</textarea>";
  body += "<br><button type=\"submit\">Save Secrets</button>";
  body += "<button type=\"submit\" formaction=\"/secrets/save-reboot\" class=\"button-gap\">Save And Reboot</button></form>";
  body += "</div></div></body></html>";

  webServer.send(200, "text/html", body);
}

void handleSecretsEditorRoot() {
  sendSecretsEditorPage("", true);
}

void handleSecretsEditorSave() {
  if (!ensureWebAuthorized()) return;

  if (!webServer.hasArg("content")) {
    sendSecretsEditorPage("No form data was received.", false);
    return;
  }

  String content = webServer.arg("content");
  if (!content.endsWith("\n")) content += "\n";

  if (!saveTextFileToSD(DEVICE_SECRETS_PATH, content)) {
    sendSecretsEditorPage("Save failed. Check the SD card and try again.", false);
    return;
  }

  loadDeviceConfigurationFromSD();
  applyTimezoneConfig();
  refreshAll();
  set_status("Secrets saved");
  sendSecretsEditorPage("Saved. Weather settings apply immediately. Reboot to fully apply Wi-Fi or hostname changes.", true);
}

void handleSecretsSaveAndReboot() {
  if (!ensureWebAuthorized()) return;

  if (!webServer.hasArg("content")) {
    sendSecretsEditorPage("No form data was received.", false);
    return;
  }

  String content = webServer.arg("content");
  if (!content.endsWith("\n")) content += "\n";

  if (!saveTextFileToSD(DEVICE_SECRETS_PATH, content)) {
    sendSecretsEditorPage("Save failed. Check the SD card and try again.", false);
    return;
  }

  loadDeviceConfigurationFromSD();
  applyTimezoneConfig();
  refreshAll();
  set_status("Rebooting...");

  String body = editorChromeStart("Rebooting", "Secrets saved. The device is rebooting now. Reconnect in a few seconds.");
  body += configSummaryHtml();
  body += "</div></div></body></html>";
  webServer.send(200, "text/html", body);
  delay(200);
  ESP.restart();
}

void sendSetupPage(const char *message, bool success) {
  bool fileOk = false;
  String existingSecrets = loadTextFileFromSD(DEVICE_SECRETS_PATH, fileOk);
  String body = editorChromeStart("Secrets Setup", "Enter Wi-Fi and web access settings so the device can join your network.");
  appendMessage(body, message, success);
  if (!fileOk) {
    body += "<div class=\"msg err\">No existing secrets.txt was found. Saving this form will create it.</div>";
  }

  body += "<form method=\"POST\" action=\"/secrets/save\">";
  body += "<label for=\"wifi_ssid\">WiFi SSID</label><input id=\"wifi_ssid\" name=\"wifi_ssid\" value=\"";
  body += htmlEscape(deviceConfig.wifiSsid);
  body += "\">";
  body += "<label for=\"wifi_password\">WiFi Password</label><input id=\"wifi_password\" name=\"wifi_password\" type=\"password\" value=\"";
  body += htmlEscape(deviceConfig.wifiPassword);
  body += "\">";
  body += "<label for=\"web_password\">Web Password</label><input id=\"web_password\" name=\"web_password\" type=\"password\" value=\"";
  body += htmlEscape(deviceConfig.webPassword);
  body += "\">";
  body += "<label for=\"owm_api_key\">OpenWeather API Key</label><input id=\"owm_api_key\" name=\"owm_api_key\" value=\"";
  body += htmlEscape(deviceConfig.owmApiKey);
  body += "\">";
  body += "<label for=\"weather_location\">Weather Location</label><input id=\"weather_location\" name=\"weather_location\" value=\"";
  body += htmlEscape(deviceConfig.weatherLocation);
  body += "\">";
  body += "<label for=\"timezone\">Timezone</label><input id=\"timezone\" name=\"timezone\" value=\"";
  body += htmlEscape(deviceConfig.timezone);
  body += "\">";
  body += "<label for=\"mdns_hostname\">mDNS Hostname</label><input id=\"mdns_hostname\" name=\"mdns_hostname\" value=\"";
  body += htmlEscape(deviceConfig.mdnsHostname);
  body += "\">";
  body += "<br><button type=\"submit\">Save Setup And Reboot</button></form>";

  if (fileOk) {
    body += "<div class=\"meta\"><p>Current /secrets.txt contents:</p><textarea readonly>";
    body += htmlEscape(existingSecrets);
    body += "</textarea></div>";
  }

  body += "</div></div></body></html>";
  webServer.send(200, "text/html", body);
}

void handleSetupRoot() {
  sendSetupPage("", true);
}

void handleSetupSave() {
  String wifiSsid = webServer.arg("wifi_ssid");
  String wifiPassword = webServer.arg("wifi_password");
  String webPassword = webServer.arg("web_password");
  String owmApiKey = webServer.arg("owm_api_key");
  String weatherLocation = webServer.arg("weather_location");
  String timezone = webServer.arg("timezone");
  String mdnsHostname = webServer.arg("mdns_hostname");

  if (wifiSsid.length() == 0 || webPassword.length() == 0) {
    sendSetupPage("WiFi SSID and web password are required.", false);
    return;
  }

  copyText(deviceConfig.wifiSsid, sizeof(deviceConfig.wifiSsid), wifiSsid.c_str());
  copyText(deviceConfig.wifiPassword, sizeof(deviceConfig.wifiPassword), wifiPassword.c_str());
  copyText(deviceConfig.webPassword, sizeof(deviceConfig.webPassword), webPassword.c_str());
  copyText(deviceConfig.owmApiKey, sizeof(deviceConfig.owmApiKey), owmApiKey.c_str(), DEFAULT_OWM_API_KEY);
  copyText(deviceConfig.weatherLocation, sizeof(deviceConfig.weatherLocation), weatherLocation.c_str(), DEFAULT_OWM_LOCATION);
  copyText(deviceConfig.timezone, sizeof(deviceConfig.timezone), timezone.c_str(), DEFAULT_TIMEZONE);
  copyText(deviceConfig.mdnsHostname, sizeof(deviceConfig.mdnsHostname), mdnsHostname.c_str(), DEFAULT_MDNS_HOSTNAME);
  sanitizeHostname(deviceConfig.mdnsHostname);
  if (deviceConfig.mdnsHostname[0] == '\0') {
    copyText(deviceConfig.mdnsHostname, sizeof(deviceConfig.mdnsHostname), DEFAULT_MDNS_HOSTNAME);
  }

  String content = buildSecretsFileFromConfig();
  if (!saveTextFileToSD(DEVICE_SECRETS_PATH, content)) {
    sendSetupPage("Save failed. Check the SD card and try again.", false);
    return;
  }

  loadDeviceConfigurationFromSD();
  applyTimezoneConfig();
  String body = editorChromeStart("Setup Saved", "The device is rebooting and will try to join your Wi-Fi network now.");
  body += "</div></div></body></html>";
  webServer.send(200, "text/html", body);
  delay(300);
  ESP.restart();
}

const char* pressureTrendLabel(PressureTrend trend) {
  switch (trend) {
    case PRESSURE_TREND_BETTER: return "Improving";
    case PRESSURE_TREND_WORSE: return "Falling";
    case PRESSURE_TREND_SAME:
    default: return "Steady";
  }
}

String formatBatteryText() {
  if (!batteryMonitorEnabled()) return "Battery monitor off";
  if (lastBatteryRaw < 0 || lastBatteryAdcMv < 0 || isnan(lastBatteryVoltage) || lastBatteryPercent < 0) {
    return "Battery unavailable";
  }

  char text[40];
  snprintf(text, sizeof(text), "%.2fV  %d%%", lastBatteryVoltage, lastBatteryPercent);
  return String(text);
}

String formatMemoryText() {
  uint32_t heapSize = ESP.getHeapSize();
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t usedHeap = heapSize > freeHeap ? heapSize - freeHeap : 0;
  int usedPercent = heapSize > 0 ? (int)roundf(((float)usedHeap * 100.0f) / (float)heapSize) : 0;

  char text[48];
  snprintf(text, sizeof(text), "%d%%  %lu/%lu KB",
           usedPercent,
           (unsigned long)(usedHeap / 1024),
           (unsigned long)(heapSize / 1024));
  return String(text);
}

String formatLocalTimeText() {
  time_t now = time(nullptr);
  if (now < 100000) return "Time unavailable";

  struct tm localTm;
  localtime_r(&now, &localTm);

  char text[32];
  strftime(text, sizeof(text), "%b %d, %Y %I:%M %p", &localTm);
  return String(text);
}

String formatCryptoPrice(float value, int decimals) {
  if (isnan(value)) return "N/A";

  char priceBuf[24];
  dtostrf(value, 0, decimals, priceBuf);
  char *trimmed = priceBuf;
  while (*trimmed == ' ') ++trimmed;

  String text = "$";
  text += trimmed;
  return text;
}

String cryptoDirectionClass(float currentVal, float prevVal) {
  if (isnan(currentVal) || isnan(prevVal)) return "flat";
  if (currentVal > prevVal) return "up";
  if (currentVal < prevVal) return "down";
  return "flat";
}

String cryptoDirectionText(float currentVal, float prevVal) {
  if (isnan(currentVal) || isnan(prevVal)) return "-";
  if (currentVal > prevVal) return "^";
  if (currentVal < prevVal) return "v";
  return "-";
}

void handleRemoteViewRoot() {
  if (!ensureWebAuthorized()) return;

  bool requestCryptoRefresh = false;
  if (WiFi.status() == WL_CONNECTED && currentPage == 0 && !cryptoWebRefreshPending &&
      (lastCryptoWebRefresh == 0 || millis() - lastCryptoWebRefresh >= cryptoWebRefreshIntervalMs)) {
    cryptoWebRefreshPending = true;
    requestCryptoRefresh = true;
  }

  String body;
  body.reserve(18000);
  body += "<!doctype html><html><head><meta charset=\"utf-8\">";
  body += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">";
  body += "<meta http-equiv=\"refresh\" content=\"";
  body += requestCryptoRefresh ? webViewRefreshAfterCryptoRequestSeconds : webViewRefreshSeconds;
  body += "\">";
  body += "<title>Cloud and Coin View</title><style>";
  body += ":root{color-scheme:dark;--bg:#0f1216;--panel:#191f26;--line:#2d3744;--text:#eef3f8;--muted:#a9b6c4;--gold:#f4b400;--blue:#7ec8ff;--green:#54d67b;--red:#ff6b6b;}";
  body += "*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font-family:Helvetica,Arial,sans-serif;}";
  body += ".wrap{max-width:1180px;margin:0 auto;padding:18px}.top{display:flex;align-items:flex-end;justify-content:space-between;gap:14px;margin-bottom:16px;}";
  body += "h1{margin:0;font-size:28px;line-height:1.1}.sub{margin:5px 0 0;color:var(--muted);font-size:14px}.nav{display:flex;gap:8px;flex-wrap:wrap}.nav a{color:#9dd7ff;text-decoration:none;border:1px solid var(--line);padding:8px 10px;border-radius:8px;background:#121820;}";
  body += ".status{display:grid;grid-template-columns:repeat(4,1fr);gap:10px;margin-bottom:14px}.pill{border:1px solid var(--line);background:#151b22;padding:12px;border-radius:8px}.pill b{display:block;font-size:12px;color:var(--muted);margin-bottom:4px;text-transform:uppercase;letter-spacing:.04em}.pill span{font-size:18px}";
  body += ".grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.screen{background:var(--panel);border:1px solid var(--line);border-radius:8px;padding:16px;min-width:0}.screen h2{margin:0 0 14px;font-size:18px;color:var(--muted)}";
  body += ".weather-main{display:grid;grid-template-columns:1fr auto;gap:14px;align-items:start}.temp{font-size:64px;font-weight:700;line-height:.95}.cond{font-size:24px;color:#d9dde3;margin-top:8px}.badge{width:78px;height:78px;border-radius:50%;display:grid;place-items:center;background:#222b34;border:1px solid var(--line);font-weight:700;color:var(--gold)}";
  body += ".metrics{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin:18px 0}.metric{border-top:1px solid var(--line);padding-top:10px}.metric b{display:block;color:var(--muted);font-size:12px;margin-bottom:4px}.metric span{font-size:20px}.hi{color:var(--gold)}.lo{color:var(--blue)}";
  body += ".forecast{display:grid;grid-template-columns:repeat(4,1fr);gap:8px}.day{border:1px solid var(--line);border-radius:8px;padding:10px;min-width:0}.day b{display:block;font-size:15px}.day .range{color:var(--gold);margin:8px 0 4px}.day .small{color:var(--muted);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}";
  body += ".crypto-list{display:grid;gap:8px}.coin{display:grid;grid-template-columns:60px 1fr 38px;gap:10px;align-items:center;border:1px solid var(--line);border-radius:8px;padding:11px 12px}.sym{font-weight:700}.price{text-align:right;font-size:20px}.dir{text-align:center;font-weight:700}.up{color:var(--green)}.down{color:var(--red)}.flat{color:var(--muted)}";
  body += "@media(max-width:760px){.wrap{padding:14px}.top{align-items:flex-start;flex-direction:column}.status{grid-template-columns:1fr}.grid{grid-template-columns:1fr}.temp{font-size:52px}.metrics{grid-template-columns:1fr 1fr}.forecast{grid-template-columns:1fr 1fr}.coin{grid-template-columns:52px 1fr 30px}.price{font-size:18px}}";
  body += "</style></head><body><div class=\"wrap\"><div class=\"top\"><div>";
  body += "<h1>Cloud and Coin</h1><p class=\"sub\">Remote display view. Refreshes every 60 seconds.</p></div>";
  body += "<div class=\"nav\"><a href=\"/view\">View</a><a href=\"/tickers\">Tickers</a><a href=\"/secrets\">Secrets</a><a href=\"/info\">Info</a></div></div>";

  body += "<div class=\"status\"><div class=\"pill\"><b>Battery</b><span>";
  body += htmlEscape(formatBatteryText());
  body += "</span></div><div class=\"pill\"><b>Memory</b><span>";
  body += htmlEscape(formatMemoryText());
  body += "</span></div><div class=\"pill\"><b>Network</b><span>";
  body += WiFi.status() == WL_CONNECTED ? htmlEscape(WiFi.localIP().toString()) : "Disconnected";
  body += "</span></div><div class=\"pill\"><b>Local Time</b><span>";
  body += htmlEscape(formatLocalTimeText());
  body += "</span></div></div>";

  body += "<div class=\"grid\"><section class=\"screen\"><h2>Weather</h2><div class=\"weather-main\"><div>";
  if (currentWeatherOk && !isnan(currentWeatherTemp)) {
    char tempText[16];
    snprintf(tempText, sizeof(tempText), "%d F", (int)round(currentWeatherTemp));
    body += "<div class=\"temp\">";
    body += tempText;
    body += "</div>";
  } else {
    body += "<div class=\"temp\">-- F</div>";
  }
  body += "<div class=\"cond\">";
  body += htmlEscape(currentWeatherOk ? String(currentWeatherCond) : String("Unavailable"));
  body += "</div></div><div class=\"badge\">";
  body += htmlEscape(String(weatherIconCode(currentWeatherCond)));
  body += "</div></div>";

  body += "<div class=\"metrics\"><div class=\"metric\"><b>High</b><span class=\"hi\">";
  if (todayHiLoOk) {
    char hiText[12];
    snprintf(hiText, sizeof(hiText), "%d F", todayHigh);
    body += hiText;
  } else {
    body += "-- F";
  }
  body += "</span></div><div class=\"metric\"><b>Low</b><span class=\"lo\">";
  if (todayHiLoOk) {
    char loText[12];
    snprintf(loText, sizeof(loText), "%d F", todayLow);
    body += loText;
  } else {
    body += "-- F";
  }
  body += "</span></div><div class=\"metric\"><b>Pressure</b><span>";
  if (currentWeatherOk && !isnan(currentWeatherPressure)) {
    char pressureText[18];
    snprintf(pressureText, sizeof(pressureText), "%d hPa", (int)round(currentWeatherPressure));
    body += pressureText;
  } else {
    body += "--";
  }
  body += "</span><div class=\"sub\">";
  body += pressureTrendLabel(currentPressureTrend);
  body += "</div></div></div>";

  body += "<div class=\"forecast\">";
  for (int i = 0; i < 4; i++) {
    body += "<div class=\"day\"><b>";
    body += htmlEscape(forecast_ok ? String(forecast[i].day) : String("---"));
    body += "</b><div class=\"range\">";
    if (forecast_ok) {
      char rangeText[18];
      snprintf(rangeText, sizeof(rangeText), "%d/%d F", forecast[i].high, forecast[i].low);
      body += rangeText;
    } else {
      body += "--/-- F";
    }
    body += "</div><div class=\"small\">";
    body += htmlEscape(forecast_ok ? String(forecast[i].cond) : String("N/A"));
    body += "</div></div>";
  }
  body += "</div></section>";

  body += "<section class=\"screen\"><h2>Crypto</h2><div class=\"crypto-list\">";
  for (int i = 0; i < configuredCryptoCount; i++) {
    String directionClass = cryptoDirectionClass(currentCryptoValues[i], previousCryptoValues[i]);
    body += "<div class=\"coin\"><div class=\"sym\">";
    body += htmlEscape(String(activeCryptos[i].symbol));
    body += "</div><div class=\"price ";
    body += directionClass;
    body += "\">";
    body += htmlEscape(formatCryptoPrice(currentCryptoValues[i], activeCryptos[i].decimals));
    body += "</div><div class=\"dir ";
    body += directionClass;
    body += "\">";
    body += cryptoDirectionText(currentCryptoValues[i], previousCryptoValues[i]);
    body += "</div></div>";
  }
  body += "</div></section></div></div></body></html>";

  webServer.send(200, "text/html", body);
}

void sendInfoPage() {
  if (!ensureWebAuthorized()) return;

  String body = editorChromeStart("Project Info", "Repository, license, notice, and contribution guidance for Cloud and Coin.");
  body += "<div class=\"section\"><h2>Attribution</h2>";
  body += "<p>Cloud and Coin is created (with a little help from AI) and licensed by <strong>Paul Hodara</strong>.</p>";
  body += "<p>Copyright 2026 Paul Hodara</p></div>";

  body += "<div class=\"section\"><h2>Repository</h2>";
  body += "<p>Project URL: <a href=\"";
  body += PROJECT_REPO_URL;
  body += "\">";
  body += PROJECT_REPO_URL;
  body += "</a></p></div>";

  body += "<div class=\"section\"><h2>License</h2>";
  body += "<p><strong>PolyForm Noncommercial License 1.0.0</strong></p>";
  body += "<p>License URL: <a href=\"https://polyformproject.org/licenses/noncommercial/1.0.0/\">https://polyformproject.org/licenses/noncommercial/1.0.0/</a></p>";
  body += "<p>Required Notice: Copyright 2026 Paul Hodara</p>";
  body += "<p>This software is available for noncommercial use under the PolyForm Noncommercial terms.</p></div>";

  body += "<div class=\"section\"><h2>Notice</h2>";
  body += "<p>Redistributions of this software must include:</p><ul>";
  body += "<li>the license text or license URL</li>";
  body += "<li>the required copyright notice for Paul Hodara</li>";
  body += "</ul></div>";

  body += "<div class=\"section\"><h2>Contributing</h2><ul>";
  body += "<li>Keep changes focused and easy to review.</li>";
  body += "<li>Do not commit secrets, tokens, API keys, or local credential files.</li>";
  body += "<li>Update documentation when behavior, setup, or configuration changes.</li>";
  body += "<li>Describe what changed and why in pull requests.</li>";
  body += "<li>Mention hardware assumptions, manual test steps, and any follow-up work still pending.</li>";
  body += "</ul></div>";

  body += "</div></div></body></html>";
  webServer.send(200, "text/html", body);
}

void handleInfoRoot() {
  sendInfoPage();
}

void handleTickerEditorNotFound() {
  webServer.sendHeader("Location", setupModeActive ? "/secrets" : "/view", true);
  webServer.send(302, "text/plain", "");
}

void startWebEditor() {
  if (setupModeActive) {
    webServer.on("/", HTTP_GET, handleSetupRoot);
    webServer.on("/setup", HTTP_GET, handleSetupRoot);
    webServer.on("/secrets", HTTP_GET, handleSetupRoot);
    webServer.on("/setup/save", HTTP_POST, handleSetupSave);
    webServer.on("/secrets/save", HTTP_POST, handleSetupSave);
  } else {
    webServer.on("/", HTTP_GET, handleRemoteViewRoot);
    webServer.on("/tickers", HTTP_GET, handleTickerEditorRoot);
    webServer.on("/tickers/save", HTTP_POST, handleTickerEditorSave);
    webServer.on("/secrets", HTTP_GET, handleSecretsEditorRoot);
    webServer.on("/secrets/save", HTTP_POST, handleSecretsEditorSave);
    webServer.on("/secrets/save-reboot", HTTP_POST, handleSecretsSaveAndReboot);
    webServer.on("/view", HTTP_GET, handleRemoteViewRoot);
    webServer.on("/info", HTTP_GET, handleInfoRoot);
  }
  webServer.onNotFound(handleTickerEditorNotFound);
  webServer.begin();
  if (setupModeActive) Serial.println("Setup editor: browse to http://192.168.4.1/secrets");
  else Serial.println("Web editor: browse to http://<device-ip>/view");
}

void startMdns() {
  if (WiFi.status() != WL_CONNECTED) return;

  if (!MDNS.begin(deviceConfig.mdnsHostname)) {
    Serial.printf("mDNS: failed to start %s.local\n", deviceConfig.mdnsHostname);
    return;
  }

  MDNS.addService("http", "tcp", 80);
  Serial.printf("mDNS: http://%s.local/\n", deviceConfig.mdnsHostname);
}

void startSetupAccessPoint() {
  setupModeActive = true;
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_SETUP_AP_NAME);
  IPAddress ip = WiFi.softAPIP();
  set_status("Setup mode");
  showSetupInstructions();
  Serial.printf("Setup AP: %s IP=%s\n", DEFAULT_SETUP_AP_NAME, ip.toString().c_str());
  startWebEditor();
}

float toFloatSafe(const char *s) {
  if (!s || !*s) return NAN;
  if (strcmp(s, "unknown") == 0 || strcmp(s, "unavailable") == 0 || strcmp(s, "None") == 0) return NAN;
  return static_cast<float>(atof(s));
}

const char* shortDayName(int weekday) {
  switch (weekday) {
    case 0: return "Sun";
    case 1: return "Mon";
    case 2: return "Tue";
    case 3: return "Wed";
    case 4: return "Thu";
    case 5: return "Fri";
    case 6: return "Sat";
    default: return "---";
  }
}

const char* weatherIconCode(const char *condIn) {
  if (containsIgnoreCase(condIn, "clear") || containsIgnoreCase(condIn, "sun")) return "SUN";
  if (containsIgnoreCase(condIn, "cloud")) return "CLD";
  if (containsIgnoreCase(condIn, "rain") || containsIgnoreCase(condIn, "drizzle")) return "RAN";
  if (containsIgnoreCase(condIn, "storm") || containsIgnoreCase(condIn, "thunder")) return "STM";
  if (containsIgnoreCase(condIn, "snow")) return "SNW";
  if (containsIgnoreCase(condIn, "mist") || containsIgnoreCase(condIn, "fog") || containsIgnoreCase(condIn, "haze")) return "FOG";
  return "N/A";
}

uint16_t weatherIconColor(const char *condIn) {
  if (containsIgnoreCase(condIn, "clear") || containsIgnoreCase(condIn, "sun")) return TFT_YELLOW;
  if (containsIgnoreCase(condIn, "cloud")) return TFT_LIGHTGREY;
  if (containsIgnoreCase(condIn, "rain") || containsIgnoreCase(condIn, "drizzle")) return TFT_BLUE;
  if (containsIgnoreCase(condIn, "storm") || containsIgnoreCase(condIn, "thunder")) return TFT_MAGENTA;
  if (containsIgnoreCase(condIn, "snow")) return TFT_WHITE;
  if (containsIgnoreCase(condIn, "mist") || containsIgnoreCase(condIn, "fog") || containsIgnoreCase(condIn, "haze")) return TFT_LIGHTGREY;
  return TFT_DARKGREY;
}
