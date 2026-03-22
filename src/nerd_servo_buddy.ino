const char* ap_ssid = "ESP32_DEV";
const char* ap_pwd  = "12345678";

#include "wifi_credentials.h"

static constexpr int S_RXD = 18;
static constexpr int S_TXD = 19;

static constexpr int S_SCL = 22;
static constexpr int S_SDA = 21;

static constexpr int RGB_LED   = 23;
static constexpr int NUMPIXELS = 10;

int max_id = 20;

String mac_address;
IPAddress ip_address;
byte   wifi_mode;
int    wifi_rssi;

static constexpr int threading_interval = 50;
static constexpr int client_interval    = 1;

#if CONFIG_FREERTOS_UNICORE
#define ARDUINO_RUNNING_CORE 0
#else
#define ARDUINO_RUNNING_CORE 1
#endif

#include "rgb_ctrl.h"
#include "preferences_config.h"
#include "sts_ctrl.h"
#include "connect.h"
#include "board_dev.h"


void setup() {
  Serial.begin(115200);
  while(!Serial) {}

  get_mac();
  
  init_board_dev();

  init_preferences();

  rgb_color(0, 64, 255);

  init_servo();

  init_wifi();

  init_web_server();

  rgb_off();

  delay(1000);
  ping_all(true);

  init_threads();
}


void loop() {
  delay(300000);
}
