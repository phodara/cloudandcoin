#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <WebServer.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <lvgl.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "secrets.h"

#define APP_VERSION "V11.4"
// Fixed four day time zone glitch
// Used VSS Codex to optimize memory and

#define TFT_BL_PIN    27
#define TOUCH_CS_PIN  33
#define SD_CS_PIN     5
#define SD_SCK_PIN    18
#define SD_MISO_PIN   19
#define SD_MOSI_PIN   23
#define TOUCH_DEBUG   0

// ---------------- Battery monitor ----------------
// This block is intentionally self-contained so it is easy to disable or remove.
// For classic ESP32 with WiFi active, prefer an ADC1 pin such as 32, 34, 35, 36, or 39.
// Never connect a LiPo directly to an ESP32 ADC pin; use a safe divider if needed.
#define ENABLE_BATTERY_MONITOR 1
#define BATTERY_ADC_PIN        34
#define BATTERY_ADC_REF_V      3.3f
#define BATTERY_DIVIDER_RATIO  2.0f
#define BATTERY_MIN_V          3.20f
#define BATTERY_MAX_V          4.20f
#define BATTERY_SAMPLE_COUNT   8
#define BATTERY_UPDATE_MS      10000UL

// ---------------- WiFi ----------------
const char* DEFAULT_WIFI_SSID = SECRET_SSID;
const char* DEFAULT_WIFI_PASS = SECRET_WIFI_PASS;
const char* DEFAULT_MDNS_HOSTNAME = "weathercrypto";

// ---------------- OpenWeather ----------------
const char* DEFAULT_OWM_API_KEY  = SECRET_OWM_API;
const char* DEFAULT_OWM_LOCATION = "Mount Kisco,US";
const char* DEFAULT_TIMEZONE = "America/New_York";
const char* DEFAULT_WEB_PASSWORD = "";
const char* DEFAULT_SETUP_AP_NAME = "weathercrypto-setup";
const char* PROJECT_REPO_URL = "https://github.com/phodara/cloudandcoin";

// ---------------- Hardware ----------------
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS_PIN);
SPIClass sdSpi(HSPI);
WebServer webServer(80);

// ---------------- LVGL ----------------
static const uint16_t screenWidth  = 480;
static const uint16_t screenHeight = 320;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t draw_buf_pixels[screenWidth * 8];

// ---------------- Touch calibration ----------------
const int touchMinX = 493;
const int touchMaxX = 3703;
const int touchMinY = 555;
const int touchMaxY = 3533;

// ---------------- Timing ----------------
unsigned long lastWeatherRefresh = 0;
const unsigned long weatherRefreshIntervalMs = 15000;

unsigned long lastCryptoPriceRefresh = 0;
const unsigned long cryptoPriceRefreshIntervalMs = 60000;

unsigned long lastHistoryRefresh = 0;
const unsigned long historyRefreshIntervalMs = 2UL * 60UL * 60UL * 1000UL;

unsigned long lastForecastRefresh = 0;
const unsigned long forecastRefreshIntervalMs = 3UL * 60UL * 60UL * 1000UL;

// ---------------- Tap detection ----------------
bool touchDown = false;
int touchDownX = 0;
int touchDownY = 0;
int touchCurrentX = 0;
int touchCurrentY = 0;
unsigned long touchDownMs = 0;
const int tapMoveThreshold = 20;
const unsigned long tapMinMs = 0;
const unsigned long tapMaxMs = 800;
bool touchDebugWasDown = false;

// ---------------- Trend memory ----------------
float prevWeatherPressure = NAN;
const char* DEVICE_SECRETS_PATH = "/secrets.txt";
const char* CRYPTO_TICKERS_PATH = "/crypto_tickers.txt";
bool sdCardReady = false;

// ---------------- Page state ----------------
int currentPage = 0;   // 0 = weather, 1 = crypto
bool cryptoSparklinesDirty = true;
bool weatherBadgesDirty = true;
bool cryptoRefreshPending = false;
bool cryptoHistoryRefreshPending = false;
bool setupModeActive = false;
int cryptoHistoryRefreshIndex = -1;
unsigned long lastCryptoHistoryStepMs = 0;

// ---------------- Sparkline history ----------------
const int HISTORY_POINTS = 30;
const int MAX_ACTIVE_CRYPTO_COUNT = 10;
const int CRYPTO_VISIBLE_ROWS = 4;
const unsigned long CRYPTO_SCROLL_INTERVAL_MS = 2500UL;
const unsigned long CRYPTO_HISTORY_STEP_INTERVAL_MS = 750UL;
float cryptoHistory[MAX_ACTIVE_CRYPTO_COUNT][HISTORY_POINTS];
bool cryptoHistoryOk[MAX_ACTIVE_CRYPTO_COUNT];
float currentCryptoValues[MAX_ACTIVE_CRYPTO_COUNT];
float previousCryptoValues[MAX_ACTIVE_CRYPTO_COUNT];
int configuredCryptoCount = 4;
int cryptoScrollOffset = 0;
unsigned long lastCryptoScrollMs = 0;

// ---------------- Weather / Forecast ----------------
struct ForecastDay {
  char day[4];
  char cond[16];
  int high;
  int low;
};

enum PressureTrend {
  PRESSURE_TREND_SAME = 0,
  PRESSURE_TREND_BETTER,
  PRESSURE_TREND_WORSE
};

struct DeviceConfig {
  char wifiSsid[64];
  char wifiPassword[64];
  char webPassword[64];
  char owmApiKey[96];
  char weatherLocation[64];
  char timezone[48];
  char mdnsHostname[32];
};

struct DeviceConfigStatus {
  bool sdCardAvailable;
  bool secretsFileFound;
  bool loadedFromSd;
  bool wifiFromSd;
  bool weatherKeyFromSd;
  bool weatherLocationFromSd;
  bool timezoneFromSd;
  bool mdnsHostnameFromSd;
};

DeviceConfig deviceConfig;
DeviceConfigStatus deviceConfigStatus;

int lastBatteryRaw = -1;
float lastBatteryVoltage = NAN;
int lastBatteryPercent = -1;
unsigned long lastBatteryReadMs = 0;

ForecastDay forecast[4];
bool forecast_ok = false;
int todayHigh = 0;
int todayLow = 0;
bool todayHiLoOk = false;
char currentWeatherCond[16] = "unknown";
PressureTrend currentPressureTrend = PRESSURE_TREND_SAME;

// ---------------- UI refs ----------------
lv_obj_t *status_label;
lv_obj_t *battery_label;

lv_obj_t *weather_page;
lv_obj_t *crypto_page;
lv_obj_t *setup_page;
lv_obj_t *setup_message_label;

lv_obj_t *weather_title_label;
lv_obj_t *weather_temp_label;
lv_obj_t *weather_cond_label;
lv_obj_t *weather_hi_label;
lv_obj_t *weather_lo_label;
lv_obj_t *weather_pressure_label;

lv_obj_t *forecast_day_label[4];
lv_obj_t *forecast_temp_label[4];
lv_obj_t *forecast_cond_label[4];

lv_obj_t *crypto_title_label;
lv_obj_t *crypto_value_labels[CRYPTO_VISIBLE_ROWS];

// Sparkline boxes
lv_obj_t *crypto_boxes[CRYPTO_VISIBLE_ROWS];

// Forecast boxes
lv_obj_t *forecast_box[4];

// ---------------- Layout constants ----------------
const int PAGE_X = 10;
const int PAGE_Y = 42;

const int PAGE_W = 460;
const int PAGE_H = 236;

const int BOX_W = 144;
const int BOX_H = 36;
const int CRYPTO_LABEL_X = 55;
const int CRYPTO_BOX_X = 296;
const int CRYPTO_ROW_LABEL_Y[CRYPTO_VISIBLE_ROWS] = {21, 71, 121, 171};
const int CRYPTO_ROW_BOX_Y[CRYPTO_VISIBLE_ROWS] = {18, 68, 118, 168};

struct CryptoDefinition {
  const char *symbol;
  const char *coinGeckoId;
  int decimals;
  uint16_t badgeColor;
  uint16_t sparkPlaceholderColor;
  char badgeChar;
};

struct ActiveCryptoConfig {
  char symbol[12];
  char coinGeckoId[32];
  int decimals;
  uint16_t badgeColor;
  uint16_t sparkPlaceholderColor;
  char badgeChar;
};

const CryptoDefinition SUPPORTED_CRYPTOS[] = {
  {"BTC", "bitcoin", 0, TFT_ORANGE, TFT_YELLOW, 'B'},
  {"ETH", "ethereum", 0, TFT_CYAN, TFT_CYAN, 'E'},
  {"ADA", "cardano", 3, TFT_BLUE, TFT_MAGENTA, 'A'},
  {"DOGE", "dogecoin", 3, TFT_GREEN, TFT_GREENYELLOW, 'D'}
};

const int SUPPORTED_CRYPTO_COUNT = sizeof(SUPPORTED_CRYPTOS) / sizeof(SUPPORTED_CRYPTOS[0]);
const uint16_t CRYPTO_BADGE_COLORS[] = {
  TFT_ORANGE, TFT_CYAN, TFT_BLUE, TFT_GREEN, TFT_YELLOW,
  TFT_MAGENTA, TFT_WHITE, TFT_RED, TFT_SKYBLUE, TFT_PINK
};
const uint16_t CRYPTO_SPARK_COLORS[] = {
  TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_GREENYELLOW, TFT_ORANGE,
  TFT_WHITE, TFT_BLUE, TFT_RED, TFT_SKYBLUE, TFT_PINK
};

ActiveCryptoConfig activeCryptos[MAX_ACTIVE_CRYPTO_COUNT];

PressureTrend evaluatePressureTrend(float currentPressure);

const CryptoDefinition* findCryptoDefinition(const char *symbol) {
  if (!symbol || !*symbol) return nullptr;
  for (int i = 0; i < SUPPORTED_CRYPTO_COUNT; i++) {
    if (strcmp(symbol, SUPPORTED_CRYPTOS[i].symbol) == 0) return &SUPPORTED_CRYPTOS[i];
  }
  return nullptr;
}

uint16_t cryptoBadgeColorForIndex(int index) {
  const int paletteSize = sizeof(CRYPTO_BADGE_COLORS) / sizeof(CRYPTO_BADGE_COLORS[0]);
  if (index < 0) index = 0;
  return CRYPTO_BADGE_COLORS[index % paletteSize];
}

uint16_t cryptoSparkColorForIndex(int index) {
  const int paletteSize = sizeof(CRYPTO_SPARK_COLORS) / sizeof(CRYPTO_SPARK_COLORS[0]);
  if (index < 0) index = 0;
  return CRYPTO_SPARK_COLORS[index % paletteSize];
}

void resetCryptoSelectionDefaults() {
  configuredCryptoCount = (SUPPORTED_CRYPTO_COUNT < CRYPTO_VISIBLE_ROWS) ? SUPPORTED_CRYPTO_COUNT : CRYPTO_VISIBLE_ROWS;
  cryptoScrollOffset = 0;

  for (int i = 0; i < MAX_ACTIVE_CRYPTO_COUNT; i++) {
    const CryptoDefinition *def = &SUPPORTED_CRYPTOS[i % SUPPORTED_CRYPTO_COUNT];
    snprintf(activeCryptos[i].symbol, sizeof(activeCryptos[i].symbol), "%s", def->symbol);
    snprintf(activeCryptos[i].coinGeckoId, sizeof(activeCryptos[i].coinGeckoId), "%s", def->coinGeckoId);
    activeCryptos[i].decimals = def->decimals;
    activeCryptos[i].badgeColor = cryptoBadgeColorForIndex(i);
    activeCryptos[i].sparkPlaceholderColor = cryptoSparkColorForIndex(i);
    activeCryptos[i].badgeChar = def->badgeChar;
    currentCryptoValues[i] = NAN;
    previousCryptoValues[i] = NAN;
    cryptoHistoryOk[i] = false;
  }
}

void setActiveCryptoFromDefinition(int index, const CryptoDefinition* def) {
  if (!def || index < 0 || index >= MAX_ACTIVE_CRYPTO_COUNT) return;
  snprintf(activeCryptos[index].symbol, sizeof(activeCryptos[index].symbol), "%s", def->symbol);
  snprintf(activeCryptos[index].coinGeckoId, sizeof(activeCryptos[index].coinGeckoId), "%s", def->coinGeckoId);
  activeCryptos[index].decimals = def->decimals;
  activeCryptos[index].badgeColor = cryptoBadgeColorForIndex(index);
  activeCryptos[index].sparkPlaceholderColor = cryptoSparkColorForIndex(index);
  activeCryptos[index].badgeChar = def->badgeChar;
}

void setActiveCryptoCustom(int index, const char *symbol, const char *coinGeckoId, int decimals) {
  if (index < 0 || index >= MAX_ACTIVE_CRYPTO_COUNT) return;

  snprintf(activeCryptos[index].symbol, sizeof(activeCryptos[index].symbol), "%s", symbol);
  snprintf(activeCryptos[index].coinGeckoId, sizeof(activeCryptos[index].coinGeckoId), "%s", coinGeckoId);
  activeCryptos[index].decimals = decimals;
  activeCryptos[index].badgeColor = cryptoBadgeColorForIndex(index);
  activeCryptos[index].sparkPlaceholderColor = cryptoSparkColorForIndex(index);
  activeCryptos[index].badgeChar = activeCryptos[index].symbol[0] ? activeCryptos[index].symbol[0] : '?';
}

lv_obj_t* cryptoValueLabelAt(int index) {
  if (index < 0 || index >= CRYPTO_VISIBLE_ROWS) return nullptr;
  return crypto_value_labels[index];
}

lv_obj_t* cryptoBoxAt(int index) {
  if (index < 0 || index >= CRYPTO_VISIBLE_ROWS) return nullptr;
  return crypto_boxes[index];
}

float* cryptoHistoryAt(int index) {
  if (index < 0 || index >= MAX_ACTIVE_CRYPTO_COUNT) return nullptr;
  return cryptoHistory[index];
}

bool* cryptoHistoryOkAt(int index) {
  if (index < 0 || index >= MAX_ACTIVE_CRYPTO_COUNT) return nullptr;
  return &cryptoHistoryOk[index];
}

float* cryptoPrevAt(int index) {
  if (index < 0 || index >= MAX_ACTIVE_CRYPTO_COUNT) return nullptr;
  return &previousCryptoValues[index];
}

bool allCryptoHistoryReady() {
  for (int i = 0; i < configuredCryptoCount; i++) {
    if (!*cryptoHistoryOkAt(i)) return false;
  }
  return true;
}

void updateCrypto();
void updateHistorySparklines();
void loadDeviceConfigurationFromSD();
void loadCryptoConfigurationFromSD();
void refreshAll(bool announceStatus = true);
void startMdns();
void startWebEditor();
void applyTimezoneConfig();
void startSetupAccessPoint();
void showSetupInstructions();
void updateBatteryStatus(bool force = false);
void renderCryptoWindow();
void updateCryptoAutoScroll();
void startCryptoHistoryRefresh();
void stepCryptoHistoryRefresh();

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

float batteryVoltageFromRaw(int raw) {
  if (raw < 0) return NAN;
  return ((float)raw / 4095.0f) * BATTERY_ADC_REF_V * BATTERY_DIVIDER_RATIO;
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
  lastBatteryVoltage = batteryVoltageFromRaw(lastBatteryRaw);
  lastBatteryPercent = batteryPercentFromVoltage(lastBatteryVoltage);

  char text[24];
  if (lastBatteryRaw < 0 || isnan(lastBatteryVoltage) || lastBatteryPercent < 0) {
    snprintf(text, sizeof(text), "Batt --");
  } else {
    snprintf(text, sizeof(text), "Batt %.2fV %d%%", lastBatteryVoltage, lastBatteryPercent);
  }
  lv_label_set_text(battery_label, text);
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

  deviceConfigStatus.sdCardAvailable = false;
  deviceConfigStatus.secretsFileFound = false;
  deviceConfigStatus.loadedFromSd = false;
  deviceConfigStatus.wifiFromSd = false;
  deviceConfigStatus.weatherKeyFromSd = false;
  deviceConfigStatus.weatherLocationFromSd = false;
  deviceConfigStatus.timezoneFromSd = false;
  deviceConfigStatus.mdnsHostnameFromSd = false;
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
  }
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
    "Secrets: loaded ssid=%s host=%s weather_location=%s timezone=%s\n",
    deviceConfig.wifiSsid[0] ? deviceConfig.wifiSsid : "(empty)",
    deviceConfig.mdnsHostname,
    deviceConfig.weatherLocation,
    deviceConfig.timezone
  );
}

const char* configSourceLabel(bool fromSd) {
  return fromSd ? "SD" : "Defaults";
}

bool webAuthRequired() {
  return deviceConfig.webPassword[0] != '\0';
}

bool ensureWebAuthorized() {
  if (!webAuthRequired()) return true;
  if (webServer.authenticate("admin", deviceConfig.webPassword)) return true;
  webServer.requestAuthentication(BASIC_AUTH, "CloudAndCoin", "Enter web password");
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
    body += "<a href=\"/tickers\">Tickers</a><a href=\"/secrets\">Secrets</a><a href=\"/info\">Info</a>";
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
  updateCrypto();
  updateHistorySparklines();
  refreshAll();
  sendTickerEditorPage("Saved. The display reloaded the ticker list from the SD card.", true);
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
  String body = editorChromeStart("Setup Mode", "Enter Wi-Fi and web access settings so the device can join your network.");
  appendMessage(body, message, success);
  if (!fileOk) {
    body += "<div class=\"msg err\">No existing secrets.txt was found. Saving this form will create it.</div>";
  }

  body += "<form method=\"POST\" action=\"/setup/save\">";
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

void sendInfoPage() {
  if (!ensureWebAuthorized()) return;

  String body = editorChromeStart("Project Info", "Repository, license, notice, and contribution guidance for WeatherCrypto.");
  body += "<div class=\"section\"><h2>Attribution</h2>";
  body += "<p>WeatherCrypto is created and licensed by <strong>Paul Hodara</strong>.</p>";
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
  webServer.sendHeader("Location", setupModeActive ? "/setup" : "/tickers", true);
  webServer.send(302, "text/plain", "");
}

void startWebEditor() {
  if (setupModeActive) {
    webServer.on("/", HTTP_GET, handleSetupRoot);
    webServer.on("/setup", HTTP_GET, handleSetupRoot);
    webServer.on("/setup/save", HTTP_POST, handleSetupSave);
  } else {
    webServer.on("/", HTTP_GET, handleTickerEditorRoot);
    webServer.on("/tickers", HTTP_GET, handleTickerEditorRoot);
    webServer.on("/tickers/save", HTTP_POST, handleTickerEditorSave);
    webServer.on("/secrets", HTTP_GET, handleSecretsEditorRoot);
    webServer.on("/secrets/save", HTTP_POST, handleSecretsEditorSave);
    webServer.on("/secrets/save-reboot", HTTP_POST, handleSecretsSaveAndReboot);
    webServer.on("/info", HTTP_GET, handleInfoRoot);
  }
  webServer.onNotFound(handleTickerEditorNotFound);
  webServer.begin();
  if (setupModeActive) Serial.println("Setup editor: browse to http://192.168.4.1/setup");
  else Serial.println("Web editor: browse to http://<device-ip>/tickers or /secrets");
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

// ---------------- Display flush ----------------
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

// ---------------- Touch read ----------------
void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;

  if (!ts.touched()) {
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  TS_Point p = ts.getPoint();

  int x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  int y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);

  data->state = LV_INDEV_STATE_PR;
  data->point.x = constrain(x, 0, screenWidth - 1);
  data->point.y = constrain(y, 0, screenHeight - 1);
}

void logTouchDebug() {
#if TOUCH_DEBUG
  if (!ts.touched()) {
    if (touchDebugWasDown) {
      Serial.println("Touch debug: released");
      touchDebugWasDown = false;
    }
    return;
  }

  TS_Point p = ts.getPoint();
  int x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  int y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);

  x = constrain(x, 0, screenWidth - 1);
  y = constrain(y, 0, screenHeight - 1);

  Serial.printf("Touch debug: raw(%d,%d,%d) mapped(%d,%d)\n", p.x, p.y, p.z, x, y);
  touchDebugWasDown = true;
#endif
}

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
}

// ---------------- CoinGecko history ----------------
bool fetchHistory30(const char* coinId, float outVals[], int &outCount) {
  outCount = 0;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("CG history: %s skipped, WiFi disconnected\n", coinId);
    return false;
  }

  HTTPClient http;
  char url[192];
  snprintf(
    url,
    sizeof(url),
    "https://api.coingecko.com/api/v3/coins/%s/market_chart?vs_currency=usd&days=30&interval=daily",
    coinId
  );

  http.begin(url);
  int code = http.GET();
  if (code <= 0) {
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

// ---------------- Page switching ----------------
void showPage(int page) {
  currentPage = page;

  if (currentPage == 0) {
    lv_obj_clear_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(weather_page);
    weatherBadgesDirty = true;
  } else {
    lv_obj_clear_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(crypto_page);
    cryptoScrollOffset = 0;
    lastCryptoScrollMs = millis();
    renderCryptoWindow();
    set_status("Updating...");
    cryptoRefreshPending = true;
    if (!allCryptoHistoryReady()) startCryptoHistoryRefresh();
    else cryptoHistoryRefreshPending = false;
    cryptoSparklinesDirty = true;
  }

  lv_refr_now(nullptr);
}

// ---------------- Tap ----------------
void handleTapToggle() {
  if (!ts.touched()) {
    if (!touchDown) return;

    int dx = abs(touchCurrentX - touchDownX);
    int dy = abs(touchCurrentY - touchDownY);
    unsigned long pressMs = millis() - touchDownMs;

#if TOUCH_DEBUG
    Serial.printf("Tap debug: dx=%d dy=%d pressMs=%lu\n", dx, dy, pressMs);
#endif

    if (dx <= tapMoveThreshold && dy <= tapMoveThreshold &&
        pressMs >= tapMinMs && pressMs <= tapMaxMs) {
#if TOUCH_DEBUG
      Serial.println("Tap debug: accepted");
#endif
      if (currentPage == 0) showPage(1);
      else showPage(0);
    } else {
#if TOUCH_DEBUG
      Serial.println("Tap debug: rejected");
#endif
    }

    touchDown = false;
    return;
  }

  TS_Point p = ts.getPoint();
  int x = map(p.x, touchMinX, touchMaxX, screenWidth - 1, 0);
  int y = map(p.y, touchMinY, touchMaxY, screenHeight - 1, 0);

  x = constrain(x, 0, screenWidth - 1);
  y = constrain(y, 0, screenHeight - 1);

  if (!touchDown) {
    touchDown = true;
    touchDownX = x;
    touchDownY = y;
    touchCurrentX = x;
    touchCurrentY = y;
    touchDownMs = millis();
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
    lv_obj_t *box = cryptoBoxAt(slot);
    int activeIndex = cryptoActiveIndexForSlot(slot);
    if (activeIndex < 0) {
      if (box) {
        lv_area_t a;
        lv_obj_get_coords(box, &a);
        tft.fillRect(a.x1 + 1, a.y1 + 1, a.x2 - a.x1 - 1, a.y2 - a.y1 - 1, TFT_BLACK);
      }
      continue;
    }
    drawSparklineInLvglBox(
      box,
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
    *cryptoHistoryOkAt(i) = fetchHistory30(activeCryptos[i].coinGeckoId, cryptoHistoryAt(i), count);
    if (i < configuredCryptoCount - 1) delay(700);
  }

  lastHistoryRefresh = millis();
  cryptoSparklinesDirty = true;
}

void startCryptoHistoryRefresh() {
  cryptoHistoryRefreshPending = configuredCryptoCount > 0;
  cryptoHistoryRefreshIndex = cryptoHistoryRefreshPending ? 0 : -1;
  lastCryptoHistoryStepMs = 0;
}

void stepCryptoHistoryRefresh() {
  if (!cryptoHistoryRefreshPending || cryptoHistoryRefreshIndex < 0) return;

  unsigned long now = millis();
  if (lastCryptoHistoryStepMs != 0 && (now - lastCryptoHistoryStepMs) < CRYPTO_HISTORY_STEP_INTERVAL_MS) {
    return;
  }

  if (currentPage == 1) set_status("Loading history...");

  int count = 0;
  int i = cryptoHistoryRefreshIndex;
  *cryptoHistoryOkAt(i) = fetchHistory30(activeCryptos[i].coinGeckoId, cryptoHistoryAt(i), count);
  cryptoSparklinesDirty = true;
  lastCryptoHistoryStepMs = now;
  cryptoHistoryRefreshIndex++;

  if (cryptoHistoryRefreshIndex >= configuredCryptoCount) {
    cryptoHistoryRefreshPending = false;
    cryptoHistoryRefreshIndex = -1;
    lastHistoryRefresh = millis();
    if (currentPage == 1) set_status("Updated");
  }
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
    "   weathercrypto-setup\n\n"
    "2. Open:\n"
    "   192.168.4.1/setup\n\n"
    "3. Add your local network settings.";

  lv_label_set_text(setup_message_label, setupText);
  lv_obj_clear_flag(setup_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(weather_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(crypto_page, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);
  lv_refr_now(nullptr);
}

// ---------------- Styling helpers ----------------
void style_screen(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x111418), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
}

void style_panel(lv_obj_t *obj) {
  lv_obj_set_style_bg_color(obj, lv_color_hex(0x1c1f24), 0);
  lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(obj, 0, 0);
  lv_obj_set_style_radius(obj, 0, 0);
  lv_obj_set_style_pad_all(obj, 12, 0);
}

// ---------------- UI build ----------------
void buildUI() {
  lv_obj_t *scr = lv_scr_act();
  style_screen(scr);

  lv_obj_t *title = lv_label_create(scr);
  char titleText[24];
  snprintf(titleText, sizeof(titleText), "Cloud & Coin v2.0");
  lv_label_set_text(title, titleText);
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(title, 14, 12);

  status_label = lv_label_create(scr);
  lv_label_set_text(status_label, "Starting...");
  lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(status_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_width(status_label, 120);
  lv_label_set_long_mode(status_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_RIGHT, 0);
  lv_obj_set_pos(status_label, 346, 14);

  battery_label = lv_label_create(scr);
  lv_label_set_text(battery_label, batteryMonitorEnabled() ? "Batt ..." : "Batt off");
  lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(battery_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_width(battery_label, 180);
  lv_label_set_long_mode(battery_label, LV_LABEL_LONG_CLIP);
  lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_LEFT, 0);
  lv_obj_set_pos(battery_label, 14, 298);

  setup_page = lv_obj_create(scr);
  lv_obj_set_size(setup_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(setup_page, PAGE_X, PAGE_Y);
  style_panel(setup_page);
  lv_obj_add_flag(setup_page, LV_OBJ_FLAG_HIDDEN);

  setup_message_label = lv_label_create(setup_page);
  lv_label_set_text(setup_message_label, "");
  lv_obj_set_width(setup_message_label, PAGE_W - 24);
  lv_label_set_long_mode(setup_message_label, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_font(setup_message_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(setup_message_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(setup_message_label, 0, 14);

  // Weather page
  weather_page = lv_obj_create(scr);
  lv_obj_set_size(weather_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(weather_page, PAGE_X, PAGE_Y);
  style_panel(weather_page);

  weather_title_label = lv_label_create(scr);
  lv_label_set_text(weather_title_label, "Weather");
  lv_obj_set_style_text_font(weather_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(weather_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(weather_title_label, LV_OBJ_FLAG_HIDDEN);

  weather_temp_label = lv_label_create(weather_page);
  lv_label_set_text(weather_temp_label, "-- F");
  lv_obj_set_style_text_font(weather_temp_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_temp_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_pos(weather_temp_label, 0, 26);

  weather_cond_label = lv_label_create(weather_page);
  lv_label_set_text(weather_cond_label, "Loading...");
  lv_obj_set_style_text_font(weather_cond_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_cond_label, lv_color_hex(0xCCCCCC), 0);
  lv_obj_set_pos(weather_cond_label, 0, 58);

  weather_hi_label = lv_label_create(weather_page);
  lv_label_set_text(weather_hi_label, "Hi: -- F");
  lv_obj_set_style_text_font(weather_hi_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_hi_label, lv_color_hex(0xF4B400), 0);
  lv_obj_set_pos(weather_hi_label, 0, 88);

  weather_lo_label = lv_label_create(weather_page);
  lv_label_set_text(weather_lo_label, "Lo: -- F");
  lv_obj_set_style_text_font(weather_lo_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_lo_label, lv_color_hex(0x7EC8FF), 0);
  lv_obj_set_pos(weather_lo_label, 110, 88);

  weather_pressure_label = lv_label_create(weather_page);
  lv_label_set_text(weather_pressure_label, "Pressure: --");
  lv_obj_set_style_text_font(weather_pressure_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(weather_pressure_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_set_pos(weather_pressure_label, 248, 88);

  for (int i = 0; i < 4; i++) {
    int x = 10 + (i * 108);
    int y = 105;

    forecast_box[i] = lv_obj_create(weather_page);
    lv_obj_remove_style_all(forecast_box[i]);
    lv_obj_set_size(forecast_box[i], 98, 92);
    lv_obj_set_pos(forecast_box[i], x, y);
    lv_obj_set_style_bg_opa(forecast_box[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(forecast_box[i], 1, 0);
    lv_obj_set_style_border_color(forecast_box[i], lv_color_hex(0x666666), 0);

    forecast_day_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_day_label[i], "---");
    lv_obj_set_style_text_font(forecast_day_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_day_label[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(forecast_day_label[i], 8, 6);

    forecast_temp_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_temp_label[i], "--/--");
    lv_obj_set_style_text_font(forecast_temp_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_temp_label[i], lv_color_hex(0xF4B400), 0);
    lv_obj_set_pos(forecast_temp_label[i], 8, 32);

    forecast_cond_label[i] = lv_label_create(forecast_box[i]);
    lv_label_set_text(forecast_cond_label[i], "N/A");
    lv_obj_set_style_text_font(forecast_cond_label[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(forecast_cond_label[i], lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_pos(forecast_cond_label[i], 8, 58);
  }

  // Crypto page
  crypto_page = lv_obj_create(scr);
  lv_obj_set_size(crypto_page, PAGE_W, PAGE_H);
  lv_obj_set_pos(crypto_page, PAGE_X, PAGE_Y);
  style_panel(crypto_page);

  crypto_title_label = lv_label_create(scr);
  lv_label_set_text(crypto_title_label, "Crypto");
  lv_obj_set_style_text_font(crypto_title_label, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(crypto_title_label, lv_color_hex(0xAAAAAA), 0);
  lv_obj_align(crypto_title_label, LV_ALIGN_TOP_MID, 0, 12);
  lv_obj_add_flag(crypto_title_label, LV_OBJ_FLAG_HIDDEN);

  for (int i = 0; i < CRYPTO_VISIBLE_ROWS; i++) {
    crypto_value_labels[i] = lv_label_create(crypto_page);
    lv_label_set_text(crypto_value_labels[i], "--");
    lv_obj_set_style_text_font(crypto_value_labels[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(crypto_value_labels[i], lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(crypto_value_labels[i], CRYPTO_LABEL_X, CRYPTO_ROW_LABEL_Y[i]);

    crypto_boxes[i] = lv_obj_create(crypto_page);
    lv_obj_remove_style_all(crypto_boxes[i]);
    lv_obj_set_size(crypto_boxes[i], BOX_W, BOX_H);
    lv_obj_set_pos(crypto_boxes[i], CRYPTO_BOX_X, CRYPTO_ROW_BOX_Y[i]);
    lv_obj_set_style_bg_opa(crypto_boxes[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(crypto_boxes[i], 1, 0);
    lv_obj_set_style_border_color(crypto_boxes[i], lv_color_hex(0x666666), 0);
  }

  showPage(0);
}

// ---------------- WiFi ----------------
void connectWiFi() {
  if (deviceConfig.wifiSsid[0] == '\0') {
    Serial.println("WiFi: SSID missing, entering setup mode");
    startSetupAccessPoint();
    return;
  }

  set_status("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(deviceConfig.wifiSsid, deviceConfig.wifiPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    set_status(deviceConfigStatus.wifiFromSd ? "WiFi SD config" : "WiFi defaults");
    Serial.printf("WiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
  } else {
    set_status("WiFi failed");
    Serial.println("WiFi: connection failed, entering setup mode");
    startSetupAccessPoint();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  if (batteryMonitorEnabled()) {
    analogReadResolution(12);
    pinMode(BATTERY_ADC_PIN, INPUT);
  }

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  SPI.begin(14, 12, 13);
  ts.begin();
  ts.setRotation(1);

  lv_init();

  lv_disp_draw_buf_init(&draw_buf, draw_buf_pixels, NULL, screenWidth * 8);

  static lv_disp_drv_t disp;
  lv_disp_drv_init(&disp);
  disp.hor_res = screenWidth;
  disp.ver_res = screenHeight;
  disp.flush_cb = my_disp_flush;
  disp.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp);

  static lv_indev_drv_t indev;
  lv_indev_drv_init(&indev);
  indev.type = LV_INDEV_TYPE_POINTER;
  indev.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev);

  buildUI();
  updateBatteryStatus(true);
  loadDeviceConfigurationFromSD();
  loadCryptoConfigurationFromSD();
  connectWiFi();
  if (!setupModeActive && WiFi.status() == WL_CONNECTED) {
    startMdns();
    startWebEditor();
  }

  if (!setupModeActive) {
    applyTimezoneConfig();
    unsigned long t0 = millis();
    while (time(nullptr) < 100000 && millis() - t0 < 10000) {
      delay(200);
    }

    refreshAll();
    fetchForecast4();
    updateForecastLabels();
    Serial.println("History: startup refresh");
    updateHistorySparklines();

    lastWeatherRefresh = millis();
  }
}

// ---------------- Loop ----------------
void loop() {
  webServer.handleClient();
  lv_timer_handler();
  updateBatteryStatus();
  updateCryptoAutoScroll();
  logTouchDebug();
  handleTapToggle();
  delay(5);

  if (setupModeActive) return;

  if (millis() - lastWeatherRefresh >= weatherRefreshIntervalMs) {
    refreshAll(currentPage == 0);
    lastWeatherRefresh = millis();
  }

  if (currentPage == 1 && cryptoRefreshPending) {
    set_status("Updating...");
    updateCrypto();
    set_status("Updated");
    lastCryptoPriceRefresh = millis();
    cryptoRefreshPending = false;
    if (!allCryptoHistoryReady()) startCryptoHistoryRefresh();
  }

  if (currentPage == 1 && cryptoHistoryRefreshPending) {
    stepCryptoHistoryRefresh();
  }

  if (currentPage == 1 && millis() - lastCryptoPriceRefresh >= cryptoPriceRefreshIntervalMs) {
    set_status("Updating...");
    updateCrypto();
    set_status("Updated");
    lastCryptoPriceRefresh = millis();
  }

  if (millis() - lastHistoryRefresh >= historyRefreshIntervalMs) {
    startCryptoHistoryRefresh();
  }

  if (!cryptoAutoScrollEnabled() && cryptoSparklinesDirty && currentPage == 1) {
    lv_refr_now(nullptr);
    drawCryptoSparklines();
  }

  if (weatherBadgesDirty && currentPage == 0) {
    lv_refr_now(nullptr);
    drawWeatherBadges();
  }
}
