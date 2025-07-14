// include/functions.h
#pragma once

// ==========================================================
// ==    DEKLARASI OBJEK GLOBAL & PROTOTYPE FUNGSI         ==
// ==========================================================
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <DHT.h>

// --- Deklarasi Eksternal (Global Objects & Variables) ---
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

enum AppState { STATE_BOOTING, STATE_AP_MODE, STATE_CONNECTING, STATE_NORMAL_OPERATION, STATE_MENU_INFO, STATE_UPDATING };
extern AppState currentState;

extern float currentHumidity, currentTemperature;
extern bool isPumpOn;
extern char mqttClientId[40];
extern int lastScheduledHour;
extern unsigned long pumpStopTime;
extern unsigned long btnOkPressTime;
extern bool okButtonLongPress;

// --- Deklarasi Fungsi (Function Prototypes) ---
void load_config();
void save_config();
void init_hardware();
void init_storage_and_wifi();
void init_mqtt();
void start_ap_mode();
void handle_connecting_state();
void handle_normal_operation();
void handle_web_root();
void handle_web_save();
void handle_main_logic();
void display_boot_screen();
void display_normal_info();
void display_menu_info();
void display_ap_info(IPAddress ip);
void display_connecting_wifi();
void check_buttons();
void reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void handle_config_update(byte* payload, unsigned int length);
void publish_telemetry();
void publish_wifi_signal();
void publish_config();
void publish_current_version();
void run_humidity_control_logic(float humidity);
void run_scheduled_control(float humidity);
void turn_pump_on(const char* reason);
void turn_pump_off();
void perform_ota_update(String url);
void publish_firmware_status(const char* status);
void trigger_email_notification(const NotificationData& data);
void check_for_firmware_update();
void publish_firmware_update_progress(const char* stage, int progress, const char* message = nullptr);
void lcd_show_message(const char* line1, const char* line2);
void pause_and_restart(unsigned long ms);
void display_wifi_failed();
void publish_online_status();
void publish_speedtest(float ping_ms, float download_mbps, float upload_mbps);