#pragma once

#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <DHT.h>

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

// Enum state aplikasi
enum AppState { STATE_BOOTING, STATE_AP_MODE, STATE_CONNECTING, STATE_NORMAL_OPERATION, STATE_MENU_INFO, STATE_UPDATING };
extern AppState currentState;

// Enum state notifikasi
enum NotifState { NOTIF_NORMAL, NOTIF_WARNING, NOTIF_CRITICAL };
extern NotifState lastNotifState;

extern float currentHumidity, currentTemperature;
extern bool isPumpOn;
extern char mqttClientId[40];
extern int lastScheduledHour;
extern unsigned long pumpStopTime;
extern unsigned long btnOkPressTime;
extern bool okButtonLongPress;
extern bool okButtonPressed;
extern unsigned long lastOkDebounceTime;
extern FirmwareInfo newFirmware;

void send_notification(const char* type, const char* message, float humidity = -1, float temperature = -1);
void publish_ota_progress(const char* stage, int percent, const char* message = "");
void publish_ota_update(String url);
void publish_firmware_status(const char* status);
void publish_current_version();
void trigger_email_notification(const NotificationData& data);
void check_for_firmware_update();

extern void turn_pump_on(const char* reason);
extern void turn_pump_off();
extern void display_menu_info();
extern void display_normal_info();