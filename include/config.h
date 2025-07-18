#pragma once

#include "secrets.h"

// ==========================================================
// ==      KONFIGURASI PROYEK JAMUR IOT                    ==
// ==========================================================

// ---------------- FIRMWARE VERSION ----------------------
#define FIRMWARE_VERSION "v28"

// ---------------- HARDWARE CONFIG -----------------------
#define LCD_ADDRESS 0x27
#define LCD_COLS 16
#define LCD_ROWS 2
#define BTN_UP_PIN 19
#define BTN_DOWN_PIN 18
#define BTN_OK_PIN 5
#define BTN_BACK_PIN 17
#define DHT_PIN 4
#define PUMP_RELAY_PIN 23
#define DHT_TYPE 11 // DHT11

// ---------------- NETWORK CONFIG ------------------------
char WIFI_SSID[33] = "";
char WIFI_PASSWORD[65] = "";
const char* AP_SSID = SECRET_AP_SSID;
const char* AP_PASSWORD = SECRET_AP_PASS;

// ---------------- MQTT CONFIG ---------------------------
const char* MQTT_BROKER = SECRET_MQTT_BROKER;
const int MQTT_PORT = 8883;
const char* MQTT_USER = SECRET_MQTT_USER;
const char* MQTT_PASSWORD = SECRET_MQTT_PASS;
const char* MQTT_CLIENT_ID_PREFIX = "jamur-iot-";
#define MQTT_CLIENT_ID_LENGTH 40
#define MQTT_KEEP_ALIVE_SEC 10

// Sertifikat Root CA untuk HiveMQ Cloud (ISRG Root X1)
const char* HIVE_MQ_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----
)EOF";

// ---------------- MQTT TOPICS ---------------------------
struct MqttTopics {
    const char* telemetry = "jamur/telemetry";
    const char* status = "jamur/status";
    const char* notification = "jamur/notifications";
    const char* pump_control = "jamur/control/pump";
    const char* config_get = "jamur/config/get";
    const char* config_set = "jamur/config/set";
    const char* wifi_signal = "jamur/wifi_signal";
    const char* system_update = "jamur/system/update";
    const char* firmware_current = "jamur/firmware/current";
    const char* firmware_new = "jamur/firmware/new_available";
    const char* firmware_update = "jamur/firmware/update";
    const char* speedtest = "jamur/speedtest";
    const char* pump_countdown = "jamur/pump/countdown";
};
const MqttTopics TOPICS;

// ---------------- TIME & SCHEDULING ---------------------
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 7 * 3600;
const int DAYLIGHT_OFFSET_SEC = 0;

// ---------------- INTERVAL & DURATION -------------------
#define PUMP_DURATION_MS 30000
#define LOGIC_CHECK_INTERVAL_MS 5000
#define WIFI_SIGNAL_PUBLISH_INTERVAL_MS 60000
#define DEBOUNCE_DELAY_MS 50
#define LONG_PRESS_MS 1500
#define NOTIF_PERIODIC_INTERVAL_MS 60000
#define NOTIF_RETRY_COUNT 3
#define NOTIF_RETRY_DELAY_MS 500
#define MQTT_RETRY_INTERVAL 5000UL
#define WIFI_RECONNECT_DELAY 10000UL
#define WIFI_CONNECT_ATTEMPTS 20
#define WIFI_CONNECT_DELAY 500
#define NTP_RETRY_DELAY 1000
#define ERROR_RESTART_DELAY 5000

// ---------------- SUPABASE CONFIG -----------------------
const char* SUPABASE_URL = SECRET_SUPABASE_URL;
const char* SUPABASE_KEY = SECRET_SUPABASE_KEY;

// ---------------- SPEEDTEST CONFIG ----------------------
#define SPEEDTEST_INTERVAL_MS 600000
#define SPEEDTEST_DOWNLOAD_URL "http://speedtest.tele2.net/1MB.zip"
#define SPEEDTEST_DOWNLOAD_SIZE (1024 * 100)
#define SPEEDTEST_UPLOAD_URL "http://httpbin.org/post"
#define SPEEDTEST_UPLOAD_SIZE (1024 * 10)
#define SPEEDTEST_RSSI_THRESHOLD -70

// ---------------- DEVICE LOCATION -----------------------
#define DEVICE_LATITUDE  -7.797068
#define DEVICE_LONGITUDE 110.370529

// ---------------- BUFFER & PAYLOAD SIZE -----------------
#define LCD_LINE_LENGTH 17
#define OTA_BUFFER_SIZE 2048
#define OTA_MAX_RETRY 3
#define OTA_HTTP_TIMEOUT_MS 30000
#define PERIODIC_MSG_SIZE 128
#define NOTIF_PAYLOAD_SIZE 256
#define TELEMETRY_PAYLOAD_SIZE 100
#define WIFI_SIGNAL_PAYLOAD_SIZE 50
#define CONFIG_BUFFER_SIZE 256
#define VERSION_PAYLOAD_SIZE 50
#define FIRMWARE_STATUS_PAYLOAD_SIZE 64
#define FIRMWARE_UPDATE_PAYLOAD_SIZE 128
#define SCHEDULE_MSG_SIZE 128
#define PUMP_MSG_SIZE 128

// ---------------- EMAIL NOTIF RATE LIMIT ----------------
#define EMAIL_MIN_INTERVAL_FIRMWARE_MS (6UL * 60 * 60 * 1000)
#define EMAIL_MIN_INTERVAL_ALERT_MS    (1UL * 60 * 60 * 1000) 
