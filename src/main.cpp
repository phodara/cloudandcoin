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
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "secrets.h"

#define APP_VERSION "V3.0"
// Fixed four day time zone glitch
// Used VSS Codex to optimize memory and

#define TFT_BL_PIN    27
#define TOUCH_CS_PIN  33
#define SD_CS_PIN     5
#define SD_SCK_PIN    18
#define SD_MISO_PIN   19
#define SD_MOSI_PIN   23

// Set to 1 to print raw touch and tap-decision details to Serial.
// This can also be overridden from platformio.ini with -D TOUCH_DEBUG=1.
#ifndef TOUCH_DEBUG
#define TOUCH_DEBUG   0
#endif

// Set to 1 to flip the landscape screen vertically (180 degrees).
// This can also be overridden from platformio.ini with -D SCREEN_FLIP_VERTICAL=1.
#ifndef SCREEN_FLIP_VERTICAL
#define SCREEN_FLIP_VERTICAL 0
#endif

// ---------------- Battery monitor ----------------
// This block is intentionally self-contained so it is easy to disable or remove.
// For classic ESP32 with WiFi active, prefer an ADC1 pin such as 32, 34, 35, 36, or 39.
// Never connect a LiPo directly to an ESP32 ADC pin; use a safe divider if needed.
#define ENABLE_BATTERY_MONITOR 1
#define BATTERY_ADC_PIN        34
#define BATTERY_DIVIDER_RATIO  2.0f
#define BATTERY_CALIBRATION    1.01f
#define BATTERY_MIN_V          3.20f
#define BATTERY_MAX_V          4.20f
#define BATTERY_CHARGE_WARNING_PERCENT 25
#define BATTERY_SAMPLE_COUNT   8
#define BATTERY_UPDATE_MS      10000UL

// ---------------- Trading signals ----------------
// Informational only. Disable to remove the Signals page.
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
const char* DEFAULT_FINNHUB_API_KEY = "";
const int MAX_WIFI_NETWORKS = 10;
const int DEFAULT_SCREEN_BRIGHTNESS_PERCENT = 100;
const int DEFAULT_CG_CURRENT_REFRESH_SECONDS = 60;
const int DEFAULT_CG_CURRENT_RETRY_MINUTES = 5;
const int DEFAULT_CG_BACKGROUND_REFRESH_MINUTES = 5;
const int DEFAULT_CG_WEB_REFRESH_SECONDS = 60;
const int DEFAULT_CG_HISTORY_STEP_SECONDS = 15;
const int DEFAULT_CG_HISTORY_RETRY_MINUTES = 5;
const int DEFAULT_CG_HISTORY_REFRESH_HOURS = 2;

// ---------------- Hardware ----------------
TFT_eSPI tft = TFT_eSPI();
XPT2046_Touchscreen ts(TOUCH_CS_PIN);
SPIClass sdSpi(HSPI);
WebServer webServer(80);
#if SCREEN_FLIP_VERTICAL
const uint8_t TFT_DISPLAY_ROTATION = 3;
#else
const uint8_t TFT_DISPLAY_ROTATION = 1;
#endif
const int TFT_BL_PWM_CHANNEL = 0;
const int TFT_BL_PWM_FREQ = 5000;
const int TFT_BL_PWM_RESOLUTION = 10;
const int TFT_BL_PWM_MAX_DUTY = (1 << TFT_BL_PWM_RESOLUTION) - 1;

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
unsigned long lastCryptoWebRefresh = 0;
unsigned long nextCryptoCurrentRetryMs = 0;
unsigned long lastStockPriceRefresh = 0;
const int webViewRefreshSeconds = 60;
const int webViewRefreshAfterCryptoRequestSeconds = 8;
const unsigned long stockPriceRefreshIntervalMs = 5UL * 60UL * 1000UL;

unsigned long lastHistoryRefresh = 0;
unsigned long nextCryptoHistoryRetryMs = 0;

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
const char* STOCK_TICKERS_PATH = "/stock_tickers.txt";
const char* NEWS_FEED_URL = "https://feeds.bbci.co.uk/news/world/rss.xml";
bool sdCardReady = false;

// ---------------- Page state ----------------
int currentPage = 0;   // 0 = weather, 1 = crypto, 2 = stocks, 3 = pair trading, 4 = signals, 5 = news, 6 = system
#if ENABLE_TRADING_SIGNALS
const int NEWS_PAGE_INDEX = 5;
const int SYSTEM_PAGE_INDEX = 6;
#else
const int NEWS_PAGE_INDEX = 4;
const int SYSTEM_PAGE_INDEX = 5;
#endif
bool cryptoSparklinesDirty = true;
bool pairTradingDirty = true;
bool tradingSignalsDirty = true;
bool weatherBadgesDirty = true;
bool cryptoRefreshPending = false;
bool cryptoWebRefreshPending = false;
bool cryptoHistoryRefreshPending = false;
bool cryptoHistoryRetryMissingOnly = false;
bool stockRefreshPending = false;
bool newsRefreshPending = false;
bool setupModeActive = false;
int cryptoHistoryRefreshIndex = -1;
unsigned long lastCryptoHistoryStepMs = 0;

// ---------------- Sparkline history ----------------
const int HISTORY_POINTS = 30;
const int MAX_ACTIVE_CRYPTO_COUNT = 10;
const int MAX_ACTIVE_STOCK_COUNT = 10;
const int CRYPTO_VISIBLE_ROWS = 4;
const int STOCK_VISIBLE_ROWS = 4;
const int PAIR_VISIBLE_ROWS = 4;
const int SIGNAL_VISIBLE_ROWS = 4;
const int MAX_NEWS_ITEMS = 8;
const int NEWS_VISIBLE_ROWS = 2;
const unsigned long CRYPTO_SCROLL_INTERVAL_MS = 2500UL;
const unsigned long STOCK_SCROLL_INTERVAL_MS = 2500UL;
const unsigned long NEWS_SCROLL_INTERVAL_MS = 60UL * 1000UL;
const unsigned long NEWS_REFRESH_INTERVAL_MS = 45UL * 60UL * 1000UL;
float cryptoHistory[MAX_ACTIVE_CRYPTO_COUNT][HISTORY_POINTS];
bool cryptoHistoryOk[MAX_ACTIVE_CRYPTO_COUNT];
float currentCryptoValues[MAX_ACTIVE_CRYPTO_COUNT];
float previousCryptoValues[MAX_ACTIVE_CRYPTO_COUNT];
int configuredCryptoCount = 4;
int cryptoScrollOffset = 0;
unsigned long lastCryptoScrollMs = 0;
float currentStockValues[MAX_ACTIVE_STOCK_COUNT];
float previousStockValues[MAX_ACTIVE_STOCK_COUNT];
float currentStockChange[MAX_ACTIVE_STOCK_COUNT];
float currentStockChangePercent[MAX_ACTIVE_STOCK_COUNT];
int configuredStockCount = 4;
int stockScrollOffset = 0;
unsigned long lastStockScrollMs = 0;

struct NewsItem {
  char title[96];
  char summary[144];
};

NewsItem currentNewsItems[MAX_NEWS_ITEMS];
int currentNewsCount = 0;
int newsScrollOffset = 0;
unsigned long lastNewsScrollMs = 0;
unsigned long lastNewsRefresh = 0;

// ---------------- Background data worker ----------------
enum DataJobType : uint8_t {
  DATA_JOB_CRYPTO_PRICES = 1,
  DATA_JOB_CRYPTO_HISTORY = 2,
  DATA_JOB_STOCK_PRICES = 3,
  DATA_JOB_NEWS = 4
};

struct DataJob {
  DataJobType type;
  int index;
  int count;
  char coinGeckoId[32];
};

QueueHandle_t dataJobQueue = nullptr;
SemaphoreHandle_t dataWorkerMutex = nullptr;
TaskHandle_t dataWorkerTaskHandle = nullptr;
bool cryptoPriceJobQueued = false;
bool cryptoPriceResultReady = false;
bool cryptoPriceResultOk = false;
bool cryptoPriceResultRateLimited = false;
int stagedCryptoPriceCount = 0;
float stagedCryptoValues[MAX_ACTIVE_CRYPTO_COUNT];
bool stagedCryptoPriceRateLimited = false;
bool cryptoHistoryJobQueued = false;
bool cryptoHistoryResultReady = false;
bool cryptoHistoryResultOk = false;
int stagedCryptoHistoryIndex = -1;
int stagedCryptoHistoryCount = 0;
float stagedCryptoHistory[HISTORY_POINTS];
bool stockPriceJobQueued = false;
bool stockPriceResultReady = false;
bool stockPriceResultOk = false;
int stagedStockPriceCount = 0;
float stagedStockValues[MAX_ACTIVE_STOCK_COUNT];
float stagedStockChange[MAX_ACTIVE_STOCK_COUNT];
float stagedStockChangePercent[MAX_ACTIVE_STOCK_COUNT];
bool newsJobQueued = false;
bool newsResultReady = false;
bool newsResultOk = false;
int stagedNewsCount = 0;
NewsItem stagedNewsItems[MAX_NEWS_ITEMS];

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
  char savedWifiSsid[MAX_WIFI_NETWORKS][64];
  char savedWifiPassword[MAX_WIFI_NETWORKS][64];
  int savedWifiCount;
  char webPassword[64];
  char owmApiKey[96];
  char finnhubApiKey[96];
  char weatherLocation[64];
  char timezone[48];
  char mdnsHostname[32];
  int screenBrightnessPercent;
  int cgCurrentRefreshSeconds;
  int cgCurrentRetryMinutes;
  int cgBackgroundRefreshMinutes;
  int cgWebRefreshSeconds;
  int cgHistoryStepSeconds;
  int cgHistoryRetryMinutes;
  int cgHistoryRefreshHours;
};

struct DeviceConfigStatus {
  bool sdCardAvailable;
  bool secretsFileFound;
  bool loadedFromSd;
  bool wifiFromSd;
  bool weatherKeyFromSd;
  bool finnhubKeyFromSd;
  bool weatherLocationFromSd;
  bool timezoneFromSd;
  bool mdnsHostnameFromSd;
  bool screenBrightnessFromSd;
  bool coinGeckoTimingFromSd;
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
lv_obj_t *stock_page;
lv_obj_t *pair_page;
lv_obj_t *signal_page;
lv_obj_t *battery_page;
lv_obj_t *news_page;
lv_obj_t *setup_page;
lv_obj_t *setup_message_label;

lv_obj_t *weather_title_label;
lv_obj_t *stock_title_label;
lv_obj_t *pair_title_label;
lv_obj_t *signal_title_label;
lv_obj_t *battery_title_label;
lv_obj_t *news_title_label;
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
lv_obj_t *stock_value_labels[STOCK_VISIBLE_ROWS];
lv_obj_t *pair_value_labels[PAIR_VISIBLE_ROWS];
lv_obj_t *signal_value_labels[SIGNAL_VISIBLE_ROWS];
lv_obj_t *battery_percent_label;
lv_obj_t *battery_voltage_label;
lv_obj_t *battery_adc_label;
lv_obj_t *battery_raw_label;
lv_obj_t *battery_range_label;
lv_obj_t *battery_state_label;
lv_obj_t *battery_level_bar;
lv_obj_t *battery_level_fill;
lv_obj_t *system_wifi_label;
lv_obj_t *system_ip_label;
lv_obj_t *system_signal_label;
lv_obj_t *system_sd_label;
lv_obj_t *system_memory_label;
lv_obj_t *system_version_label;
lv_obj_t *news_headline_labels[NEWS_VISIBLE_ROWS];
lv_obj_t *news_summary_labels[NEWS_VISIBLE_ROWS];
lv_obj_t *news_status_label;

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

struct ActiveStockConfig {
  char symbol[12];
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
ActiveStockConfig activeStocks[MAX_ACTIVE_STOCK_COUNT];

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
void loadStockConfigurationFromSD();
void refreshAll(bool announceStatus = true);
void startMdns();
void startWebEditor();
void applyTimezoneConfig();
void startSetupAccessPoint();
void showSetupInstructions();
void updateBatteryStatus(bool force = false);
void initBacklightControl();
void applyScreenBrightness();
void handleRemoteViewRoot();
const char* weatherIconCode(const char *condIn);
String formatMemoryText();
void renderCryptoWindow();
void renderStockWindow();
void renderNewsWindow();
void updateCryptoAutoScroll();
void updateStockAutoScroll();
void updateNewsAutoScroll();
void startCryptoHistoryRefresh();
void stepCryptoHistoryRefresh();
void startDataWorker();
bool queueCryptoPriceRefresh();
bool queueCryptoHistoryRefresh(int index);
bool queueStockPriceRefresh();
bool queueNewsRefresh();
bool dataWorkerBusy();
void applyDataWorkerResults();
void drawCryptoSparklines();
void pairTradingRender();
void tradingSignalsRender();

#include "app/helpers.inc"
#include "app/pair_trading.inc"
#include "app/trading_signal.inc"
#include "app/news_data.inc"
#include "app/web.inc"
#include "app/display_touch.inc"
#include "app/crypto_data.inc"
#include "app/stock_data.inc"
#include "app/weather_data.inc"
#include "app/runtime_ui.inc"
#include "app/ui_build.inc"
// ---------------- WiFi ----------------
bool tryConnectConfiguredWifi(int wifiIndex) {
  if (wifiIndex < 0 || wifiIndex >= MAX_WIFI_NETWORKS) return false;
  if (deviceConfig.savedWifiSsid[wifiIndex][0] == '\0') return false;

  copyText(deviceConfig.wifiSsid, sizeof(deviceConfig.wifiSsid), deviceConfig.savedWifiSsid[wifiIndex]);
  copyText(deviceConfig.wifiPassword, sizeof(deviceConfig.wifiPassword), deviceConfig.savedWifiPassword[wifiIndex]);

  set_status("Connecting WiFi...");
  Serial.printf("WiFi: trying configured network %d, SSID=%s\n", wifiIndex + 1, deviceConfig.wifiSsid);
  WiFi.begin(deviceConfig.wifiSsid, deviceConfig.wifiPassword);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    set_status(deviceConfigStatus.wifiFromSd ? "WiFi SD config" : "WiFi defaults");
    Serial.printf("WiFi: connected to %s, IP=%s\n", deviceConfig.wifiSsid, WiFi.localIP().toString().c_str());
    return true;
  }

  Serial.printf("WiFi: failed to connect to %s\n", deviceConfig.wifiSsid);
  WiFi.disconnect(false, false);
  delay(200);
  return false;
}

void connectWiFi() {
  if (deviceConfig.savedWifiCount == 0) {
    Serial.println("WiFi: SSID missing, entering setup mode");
    startSetupAccessPoint();
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(false, false);
  delay(100);

  set_status("Scanning WiFi...");
  int networkCount = WiFi.scanNetworks(false, true);
  bool attempted[MAX_WIFI_NETWORKS] = { false };

  if (networkCount > 0) {
    Serial.printf("WiFi: scan found %d networks\n", networkCount);
    for (int found = 0; found < networkCount; found++) {
      String foundSsid = WiFi.SSID(found);
      for (int saved = 0; saved < MAX_WIFI_NETWORKS; saved++) {
        if (attempted[saved] || deviceConfig.savedWifiSsid[saved][0] == '\0') continue;
        if (foundSsid == deviceConfig.savedWifiSsid[saved]) {
          attempted[saved] = true;
          if (tryConnectConfiguredWifi(saved)) {
            WiFi.scanDelete();
            return;
          }
        }
      }
    }
  } else {
    Serial.printf("WiFi: scan found no networks, result=%d\n", networkCount);
  }

  WiFi.scanDelete();

  for (int saved = 0; saved < MAX_WIFI_NETWORKS; saved++) {
    if (attempted[saved] || deviceConfig.savedWifiSsid[saved][0] == '\0') continue;
    if (tryConnectConfiguredWifi(saved)) return;
  }

  set_status("WiFi failed");
  Serial.println("WiFi: all configured networks failed, entering setup mode");
  startSetupAccessPoint();
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
  tft.setRotation(TFT_DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  initBacklightControl();

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
  applyScreenBrightness();
  loadCryptoConfigurationFromSD();
  loadStockConfigurationFromSD();
  connectWiFi();
  startDataWorker();
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
    set_status("Crypto updating");
    if (queueCryptoPriceRefresh()) lastCryptoPriceRefresh = millis();
    startCryptoHistoryRefresh();
    set_status("Stocks updating");
    if (queueStockPriceRefresh()) lastStockPriceRefresh = millis();

    lastWeatherRefresh = millis();
  }
}

// ---------------- Loop ----------------
void loop() {
  webServer.handleClient();
  lv_timer_handler();
  applyDataWorkerResults();
  updateBatteryStatus();
  updateCryptoAutoScroll();
  updateStockAutoScroll();
  updateNewsAutoScroll();
#if TOUCH_DEBUG
  logTouchDebug();
#endif
  handleTapToggle();
  delay(5);

  if (setupModeActive) return;

  bool touchSettledForNetwork = millis() - lastTouchInteractionMs >= touchNetworkSettleMs;

  if (cryptoWebRefreshPending && !cryptoCurrentBackoffActive()) {
    if (queueCryptoPriceRefresh()) {
      set_status("Crypto updating");
      lastCryptoPriceRefresh = millis();
      lastCryptoWebRefresh = lastCryptoPriceRefresh;
      cryptoWebRefreshPending = false;
    }
  }

  if (touchSettledForNetwork && !dataWorkerBusy() && millis() - lastWeatherRefresh >= weatherRefreshIntervalMs) {
    refreshAll(currentPage == 0);
    lastWeatherRefresh = millis();
  }

  if (touchSettledForNetwork && !cryptoHistoryRefreshPending && !cryptoCurrentBackoffActive() && currentPageUsesCryptoRefresh() && cryptoRefreshPending) {
    set_status("Crypto updating");
    if (queueCryptoPriceRefresh()) {
      lastCryptoPriceRefresh = millis();
      cryptoRefreshPending = false;
      if (!allCryptoHistoryReady() && !cryptoHistoryRefreshPending) startCryptoHistoryRefresh();
    }
  }

  if (touchSettledForNetwork && currentPage == 2 && stockRefreshPending) {
    set_status("Stocks updating");
    if (queueStockPriceRefresh()) {
      lastStockPriceRefresh = millis();
      stockRefreshPending = false;
    }
  }

  if (touchSettledForNetwork && newsRefreshPending) {
    if (currentPage == NEWS_PAGE_INDEX) set_status("News updating");
    if (queueNewsRefresh()) {
      lastNewsRefresh = millis();
      newsRefreshPending = false;
    }
  }

  if (touchSettledForNetwork && cryptoHistoryRefreshPending) {
    stepCryptoHistoryRefresh();
  }

  if (touchSettledForNetwork && !cryptoHistoryRefreshPending && !cryptoCurrentBackoffActive() && currentPageUsesCryptoRefresh() && millis() - lastCryptoPriceRefresh >= cryptoPriceRefreshIntervalMs()) {
    set_status("Crypto updating");
    if (queueCryptoPriceRefresh()) lastCryptoPriceRefresh = millis();
  }

  if (touchSettledForNetwork && millis() - lastStockPriceRefresh >= stockPriceRefreshIntervalMs) {
    if (currentPage == 2) set_status("Stocks updating");
    if (queueStockPriceRefresh()) lastStockPriceRefresh = millis();
  }

  if (touchSettledForNetwork && !newsRefreshPending && millis() - lastNewsRefresh >= NEWS_REFRESH_INTERVAL_MS) {
    if (currentPage == NEWS_PAGE_INDEX) set_status("News updating");
    if (queueNewsRefresh()) lastNewsRefresh = millis();
  }

  if (touchSettledForNetwork && !cryptoHistoryRefreshPending && !cryptoCurrentBackoffActive() && currentPage == 0 && millis() - lastCryptoPriceRefresh >= cryptoBackgroundRefreshIntervalMs()) {
    if (queueCryptoPriceRefresh()) lastCryptoPriceRefresh = millis();
  }

  if (millis() - lastHistoryRefresh >= historyRefreshIntervalMs()) {
    startCryptoHistoryRefresh();
  }

  if (!cryptoAutoScrollEnabled() && cryptoSparklinesDirty && currentPage == 1) {
    lv_refr_now(nullptr);
    drawCryptoSparklines();
  }

  if (pairTradingDirty && currentPage == 3) {
    pairTradingRender();
  }

#if ENABLE_TRADING_SIGNALS
  if (tradingSignalsDirty && currentPage == 4) {
    tradingSignalsRender();
  }
#endif

  if (weatherBadgesDirty && currentPage == 0) {
    lv_refr_now(nullptr);
    drawWeatherBadges();
  }
}
