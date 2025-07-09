#pragma once

#include <stdint.h>
#include "secrets.h"

// ==========================================================
// ==      FILE KONFIGURASI PROYEK JAMUR IOT               ==
// ==========================================================

#define FIRMWARE_VERSION "v23.6"

// --- KONFIGURASI PERANGKAT KERAS LOKAL (Layar & Tombol) ---
constexpr uint8_t LCD_ADDRESS = 0x27;
constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;
constexpr uint8_t BTN_UP_PIN = 19;
constexpr uint8_t BTN_DOWN_PIN = 18;
constexpr uint8_t BTN_OK_PIN = 5;
constexpr uint8_t BTN_BACK_PIN = 17;

// --- KONFIGURASI PERANGKAT KERAS SENSOR & AKTUATOR ---
constexpr uint8_t DHT_PIN = 4;
constexpr uint8_t PUMP_RELAY_PIN = 23;
constexpr uint8_t DHT_TYPE = 11; // DHT11

// --- KONFIGURASI JARINGAN ---
extern char WIFI_SSID[33];
extern char WIFI_PASSWORD[65];
constexpr const char* AP_SSID = SECRET_AP_SSID;
constexpr const char* AP_PASSWORD = SECRET_AP_PASS;

// --- KONFIGURASI MQTT ---
constexpr const char* MQTT_BROKER = SECRET_MQTT_BROKER;
constexpr int MQTT_PORT = 8883;
constexpr const char* MQTT_USER = SECRET_MQTT_USER;
constexpr const char* MQTT_PASSWORD = SECRET_MQTT_PASS;
constexpr const char* MQTT_CLIENT_ID_PREFIX = "jamur-iot-";

// --- Sertifikat Root CA untuk HiveMQ Cloud (ISRG Root X1) ---
extern const char* HIVE_MQ_ROOT_CA;

// --- STRUKTUR TOPIK MQTT ---
struct MqttTopics {
    const char* telemetry = "jamur/telemetry";
    const char* status = "jamur/status";
    const char* notification = "jamur/notifications";
    const char* pump_control = "jamur/control/pump";
    const char* config_get = "jamur/config/get";
    const char* config_set = "jamur/config/set";
    const char* wifi_signal = "jamur/wifi_signal";
    const char* system_update = "jamur/system/update";
    const char* firmware_update = "jamur/firmware/update";
    const char* firmware_current = "jamur/firmware/current";
    const char* firmware_new = "jamur/firmware/new_available";
};
const MqttTopics TOPICS;

// --- KONFIGURASI PENJADWALAN & WAKTU ---
constexpr const char* NTP_SERVER = "pool.ntp.org";
constexpr long GMT_OFFSET_SEC = 7 * 3600;
constexpr int DAYLIGHT_OFFSET_SEC = 0;

// --- Durasi & Interval (dalam milidetik) ---
constexpr int PUMP_DURATION_MS = 30000;
constexpr int LOGIC_CHECK_INTERVAL_MS = 5000;
constexpr int WIFI_SIGNAL_PUBLISH_INTERVAL_MS = 60000;
constexpr int DEBOUNCE_DELAY_MS = 50;
constexpr int LONG_PRESS_MS = 1500;
constexpr int NOTIF_PERIODIC_INTERVAL_MS = 60000; // 1 menit
constexpr int NOTIF_RETRY_COUNT = 3;
constexpr int NOTIF_RETRY_DELAY_MS = 500;
constexpr unsigned long MQTT_RETRY_INTERVAL = 5000; // 5 detik

// --- KONFIGURASI SUPABASE EMAIL NOTIFIKASI ---
constexpr const char* SUPABASE_URL = SECRET_SUPABASE_URL;
constexpr const char* SUPABASE_KEY = SECRET_SUPABASE_KEY; 