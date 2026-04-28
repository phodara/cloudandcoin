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
#include <esp_ota_ops.h>
#include <esp_partition.h>
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
#define TOUCH_DEBUG   1

// ---------------- Battery monitor ----------------
// This block is intentionally self-contained so it is easy to disable or remove.
// For classic ESP32 with WiFi active, prefer an ADC1 pin such as 32, 34, 35, 36, or 39.
// Never connect a LiPo directly to an ESP32 ADC pin; use a safe divider if needed.
#define ENABLE_BATTERY_MONITOR 1
#define BATTERY_ADC_PIN        34
#define BATTERY_DIVIDER_RATIO  2.0f
#define BATTERY_CALIBRATION    1.0f
#define BATTERY_MIN_V          3.20f
#define BATTERY_MAX_V          4.20f
#define BATTERY_SAMPLE_COUNT   8
#define BATTERY_UPDATE_MS      10000UL

// ---------------- Trading signals ----------------
// Informational only. Disable to return to the three-screen app.
#define ENABLE_TRADING_SIGNALS 1

// ---------------- WiFi ----------------
const char* DEFAULT_WIFI_SSID = SECRET_SSID;
const char* DEFAULT_WIFI_PASS = SECRET_WIFI_PASS;
const char* DEFAULT_MDNS_HOSTNAME = "cloudandcoin";

// ---------------- OpenWeather ----------------
const char* DEFAULT_OWM_API_KEY  = SECRET_OWM_API;
const char* DEFAULT_OWM_LOCATION = "Mount Kisco,US";
const char* DEFAULT_TIMEZONE = "America/New_York";
const char* DEFAULT_WEB_PASSWORD = "";
const char* DEFAULT_SETUP_AP_NAME = "cloudandcoin-setup";
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
const int touchPressureMin = 80;

// ---------------- Timing ----------------
unsigned long lastWeatherRefresh = 0;
const unsigned long weatherRefreshIntervalMs = 15000;

unsigned long lastCryptoPriceRefresh = 0;
const unsigned long cryptoPriceRefreshIntervalMs = 60000;
const unsigned long cryptoBackgroundRefreshIntervalMs = 5UL * 60UL * 1000UL;
unsigned long lastCryptoWebRefresh = 0;
const unsigned long cryptoWebRefreshIntervalMs = 60000;
const int webViewRefreshSeconds = 60;
const int webViewRefreshAfterCryptoRequestSeconds = 8;

unsigned long lastHistoryRefresh = 0;
const unsigned long historyRefreshIntervalMs = 2UL * 60UL * 60UL * 1000UL;
unsigned long nextCryptoHistoryRetryMs = 0;
const unsigned long cryptoHistoryRetryIntervalMs = 30000UL;

unsigned long lastForecastRefresh = 0;
const unsigned long forecastRefreshIntervalMs = 3UL * 60UL * 60UL * 1000UL;

// ---------------- Tap detection ----------------
bool touchDown = false;
bool touchNavigatedOnPress = false;
int touchDownX = 0;
int touchDownY = 0;
int touchCurrentX = 0;
int touchCurrentY = 0;
unsigned long touchDownMs = 0;
const int tapMoveThreshold = 20;
const unsigned long tapMinMs = 0;
const unsigned long tapMaxMs = 800;
bool touchDebugWasDown = false;
unsigned long lastTouchDebugMs = 0;
unsigned long lastTouchInteractionMs = 0;
const unsigned long touchNetworkSettleMs = 750;

// ---------------- Trend memory ----------------
float prevWeatherPressure = NAN;
const char* DEVICE_SECRETS_PATH = "/secrets.txt";
const char* CRYPTO_TICKERS_PATH = "/crypto_tickers.txt";
bool sdCardReady = false;

// ---------------- Page state ----------------
int currentPage = 0;   // 0 = weather, 1 = crypto, 2 = pair trading, 3 = signals
bool cryptoSparklinesDirty = true;
bool pairTradingDirty = true;
bool tradingSignalsDirty = true;
bool weatherBadgesDirty = true;
bool cryptoRefreshPending = false;
bool cryptoWebRefreshPending = false;
bool cryptoHistoryRefreshPending = false;
bool cryptoHistoryRetryMissingOnly = false;
bool setupModeActive = false;
int cryptoHistoryRefreshIndex = -1;
unsigned long lastCryptoHistoryStepMs = 0;

// ---------------- Sparkline history ----------------
const int HISTORY_POINTS = 30;
const int MAX_ACTIVE_CRYPTO_COUNT = 10;
const int CRYPTO_VISIBLE_ROWS = 4;
const int PAIR_VISIBLE_ROWS = 4;
const int SIGNAL_VISIBLE_ROWS = 4;
const unsigned long CRYPTO_SCROLL_INTERVAL_MS = 2500UL;
const unsigned long CRYPTO_HISTORY_STEP_INTERVAL_MS = 3000UL;
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
int lastBatteryAdcMv = -1;
float lastBatteryVoltage = NAN;
int lastBatteryPercent = -1;
unsigned long lastBatteryReadMs = 0;

ForecastDay forecast[4];
bool forecast_ok = false;
bool currentWeatherOk = false;
float currentWeatherTemp = NAN;
float currentWeatherPressure = NAN;
int todayHigh = 0;
int todayLow = 0;
bool todayHiLoOk = false;
char currentWeatherCond[16] = "unknown";
PressureTrend currentPressureTrend = PRESSURE_TREND_SAME;

// ---------------- UI refs ----------------
lv_obj_t *status_label;
lv_obj_t *battery_label;
lv_obj_t *memory_label;

lv_obj_t *weather_page;
lv_obj_t *crypto_page;
lv_obj_t *pair_page;
lv_obj_t *signal_page;
lv_obj_t *setup_page;
lv_obj_t *setup_message_label;

lv_obj_t *weather_title_label;
lv_obj_t *pair_title_label;
lv_obj_t *signal_title_label;
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
lv_obj_t *pair_value_labels[PAIR_VISIBLE_ROWS];
lv_obj_t *signal_value_labels[SIGNAL_VISIBLE_ROWS];

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
void handleRemoteViewRoot();
const char* weatherIconCode(const char *condIn);
String formatMemoryText();
void renderCryptoWindow();
void updateCryptoAutoScroll();
void startCryptoHistoryRefresh();
void stepCryptoHistoryRefresh();
void drawCryptoSparklines();
void pairTradingRender();
void tradingSignalsRender();

#include "app/helpers.inc"
#include "app/pair_trading.inc"
#include "app/trading_signal.inc"
#include "app/web.inc"
#include "app/display_touch.inc"
#include "app/crypto_data.inc"
#include "app/weather_data.inc"
#include "app/runtime_ui.inc"
#include "app/ui_build.inc"
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
    analogSetPinAttenuation(BATTERY_ADC_PIN, ADC_11db);
    pinMode(BATTERY_ADC_PIN, INPUT);
  }

  pinMode(TFT_BL_PIN, OUTPUT);
  digitalWrite(TFT_BL_PIN, HIGH);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  SPI.begin(14, 12, 13);
  ts.begin();
  SPI.begin(14, 12, 13);
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
    updateCrypto();
    lastCryptoPriceRefresh = millis();
    startCryptoHistoryRefresh();

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

  bool touchSettledForNetwork = millis() - lastTouchInteractionMs >= touchNetworkSettleMs;

  if (cryptoWebRefreshPending) {
    updateCrypto();
    lastCryptoPriceRefresh = millis();
    lastCryptoWebRefresh = lastCryptoPriceRefresh;
    cryptoWebRefreshPending = false;
  }

  if (touchSettledForNetwork && millis() - lastWeatherRefresh >= weatherRefreshIntervalMs) {
    refreshAll(currentPage == 0);
    lastWeatherRefresh = millis();
  }

  if (touchSettledForNetwork && (currentPage == 1 || currentPage == 2 || currentPage == 3) && cryptoRefreshPending) {
    set_status("Updating...");
    updateCrypto();
    pairTradingDirty = true;
    tradingSignalsDirty = true;
    if (currentPage == 2) pairTradingRender();
    if (currentPage == 3) tradingSignalsRender();
    set_status("Updated");
    lastCryptoPriceRefresh = millis();
    cryptoRefreshPending = false;
    if (!allCryptoHistoryReady() && !cryptoHistoryRefreshPending) startCryptoHistoryRefresh();
  }

  if (touchSettledForNetwork && (currentPage == 1 || currentPage == 2 || currentPage == 3) && cryptoHistoryRefreshPending) {
    stepCryptoHistoryRefresh();
  }

  if (touchSettledForNetwork && (currentPage == 1 || currentPage == 2 || currentPage == 3) && millis() - lastCryptoPriceRefresh >= cryptoPriceRefreshIntervalMs) {
    set_status("Updating...");
    updateCrypto();
    pairTradingDirty = true;
    tradingSignalsDirty = true;
    if (currentPage == 2) pairTradingRender();
    if (currentPage == 3) tradingSignalsRender();
    set_status("Updated");
    lastCryptoPriceRefresh = millis();
  }

  if (touchSettledForNetwork && currentPage == 0 && millis() - lastCryptoPriceRefresh >= cryptoBackgroundRefreshIntervalMs) {
    updateCrypto();
    pairTradingDirty = true;
    tradingSignalsDirty = true;
    lastCryptoPriceRefresh = millis();
  }

  if (millis() - lastHistoryRefresh >= historyRefreshIntervalMs) {
    startCryptoHistoryRefresh();
  }

  if (!cryptoAutoScrollEnabled() && cryptoSparklinesDirty && currentPage == 1) {
    lv_refr_now(nullptr);
    drawCryptoSparklines();
  }

  if (pairTradingDirty && currentPage == 2) {
    pairTradingRender();
  }

  if (tradingSignalsDirty && currentPage == 3) {
    tradingSignalsRender();
  }

  if (weatherBadgesDirty && currentPage == 0) {
    lv_refr_now(nullptr);
    drawWeatherBadges();
  }
}
