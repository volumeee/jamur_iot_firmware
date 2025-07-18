// include/functions.h
#pragma once

// ==========================================================
// ==    DEKLARASI OBJEK GLOBAL, STRUCT, ENUM, PROTOTYPE   ==
// ==========================================================
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <DHT.h>

// ---------------- GLOBAL OBJECTS & VARIABLES -------------
class WebServer;
extern WebServer server;
extern WiFiClientSecure espClient;
extern PubSubClient mqttClient;
extern LiquidCrystal_I2C lcd;
extern Preferences preferences;
extern DHT dht;

struct DeviceConfig {
    float humidity_critical;
    float humidity_warning;
    int schedule_hours[5];
    int schedule_count;
};

struct NotificationData {
    const char* type;
    const char* message;
    float humidity = -1;
    float temperature = -1;
    String version = "";
    String release_notes = "";
};

struct FirmwareInfo {
    String version = "";
    String release_notes = "";
    String url = "";
};

extern DeviceConfig config;

enum AppState {
    STATE_BOOTING,
    STATE_AP_MODE,
    STATE_CONNECTING,
    STATE_NORMAL_OPERATION,
    STATE_MENU_INFO,
    STATE_UPDATING
};
extern AppState currentState;

extern float currentHumidity, currentTemperature;
extern bool isPumpOn;
extern char mqttClientId[40];
extern int lastScheduledHour;
extern unsigned long pumpStopTime;
extern unsigned long btnOkPressTime;
extern bool okButtonLongPress;

// ---------------- FUNCTION PROTOTYPES --------------------
// Initialization
void init_hardware();
void load_config();
void save_config();
void init_storage_and_wifi();
void init_mqtt();

// Main
void setup();
void loop();

// State Management
void handle_connecting_state();
void handle_normal_operation();
void start_ap_mode();
void handle_web_root();
void handle_web_save();

// Display
void display_boot_screen();
void display_connecting_wifi();
void display_wifi_failed();
void display_ap_info(IPAddress ip);
void display_normal_info();
void display_menu_info();

// Input
void check_buttons();

// Control Logic
void handle_main_logic();
void run_humidity_control_logic(float humidity);
void run_scheduled_control(float humidity);
void turn_pump_on(const char* reason);
void turn_pump_off();
void publish_pump_countdown(int seconds);
void update_pump_countdown();

// Communication
void try_reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void handle_config_update(byte* payload, unsigned int length);
bool publish_with_retry(const char* topic, const char* payload, bool retained = false, int retry = 3, int delayMs = 500);
void send_notification(const char* type, const char* message, float humidity = -1, float temperature = -1);
void publish_telemetry();
void publish_wifi_signal();
void publish_config();
void publish_current_version();
void publish_firmware_status(const char* status);
void publish_firmware_update_progress(const char* stage, int progress, const char* message = nullptr);
void trigger_email_notification(const NotificationData& data);
void check_for_firmware_update();

// OTA Update
void perform_ota_update(String url);
void handle_ota_error(const char* lcdMsg, const char* logMsg, int code = 0);

// Speedtest
float speedtest_ping_ms(const char* host = "8.8.8.8", uint16_t port = 53, uint8_t count = 4);
float speedtest_download_mbps(const char* url = SPEEDTEST_DOWNLOAD_URL, size_t test_size = SPEEDTEST_DOWNLOAD_SIZE);
float speedtest_upload_mbps(const char* url = SPEEDTEST_UPLOAD_URL, size_t test_size = SPEEDTEST_UPLOAD_SIZE);
void run_and_publish_speedtest();
void publish_speedtest(float ping_ms, float download_mbps, float upload_mbps);

// Utility
void lcd_show_message(const char* line1, const char* line2);
void pause_and_restart(unsigned long ms);
void publish_online_status();
bool can_send_email(unsigned long& lastSent, unsigned long minInterval);