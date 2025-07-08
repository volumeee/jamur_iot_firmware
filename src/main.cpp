// =================================================================
//   FIRMWARE KONTROL BUDIDAYA JAMUR TIRAM V17.3 (ArduinoJson v7 Fix)
//   Oleh: bagus-erwanto.vercel.app
//   Tanggal: 6 Juli 2025
// =================================================================

// --- Library & File Pendukung (URUTAN INI PENTING) ---
#include <WiFi.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <DHT.h>
#include "time.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

#include "config.h"
#include "functions.h"

// =================================================================
//   DEKLARASI OBJEK & VARIABEL GLOBAL
// =================================================================
WebServer server(80);
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
DHT dht(DHT_PIN, DHT_TYPE);
LiquidCrystal_I2C lcd(LCD_ADDRESS, LCD_COLS, LCD_ROWS);
Preferences preferences;

DeviceConfig config;
AppState currentState = STATE_BOOTING;

float currentHumidity = 0.0, currentTemperature = 0.0;
bool isPumpOn = false;
char mqttClientId[40];

unsigned long lastLogicCheckTime = 0, lastWifiSignalPublishTime = 0, pumpStopTime = 0;
int lastScheduledHour = -1;
unsigned long btnOkPressTime = 0;
bool okButtonLongPress = false;
bool isWarningZone = false;
bool okButtonPressed = false;

unsigned long lastPeriodicNotifTime = 0;

// Tambahkan enum dan variabel untuk status notifikasi terakhir
enum NotifState { NOTIF_NORMAL, NOTIF_WARNING, NOTIF_CRITICAL };
NotifState lastNotifState = NOTIF_NORMAL;

// Helper function: publish MQTT dengan retry
bool publish_with_retry(const char* topic, const char* payload, bool retained = false, int retry = NOTIF_RETRY_COUNT, int delayMs = NOTIF_RETRY_DELAY_MS) {
    for (int i = 0; i < retry; ++i) {
        if (mqttClient.publish(topic, payload, retained)) {
            return true;
        }
        delay(delayMs);
    }
    return false;
}

// Helper function: kirim notifikasi dengan format JSON
void send_notification(const char* type, const char* message, float humidity = -1, float temperature = -1) {
    char notifPayload[256];
    if (humidity >= 0 && temperature >= 0) {
        snprintf(notifPayload, sizeof(notifPayload),
            "{\"type\":\"%s\", \"message\":\"%s\", \"humidity\":%.1f, \"temperature\":%.1f}",
            type, message, humidity, temperature);
    } else {
        snprintf(notifPayload, sizeof(notifPayload),
            "{\"type\":\"%s\", \"message\":\"%s\"}", type, message);
    }
    publish_with_retry(TOPICS.notification, notifPayload);
}

// Helper class untuk Preferences (ConfigStorage)
class ConfigStorage {
public:
    void load(DeviceConfig& cfg) {
        Preferences prefs;
        prefs.begin("jamur-config", true);
        cfg.humidity_critical = prefs.getFloat("h_crit", 80.0);
        cfg.humidity_warning = prefs.getFloat("h_warn", 85.0);
        cfg.schedule_count = prefs.getBytes("schedules", cfg.schedule_hours, sizeof(cfg.schedule_hours)) / sizeof(int);
        if (cfg.schedule_count == 0) {
            int default_schedule[] = {7, 12, 17};
            memcpy(cfg.schedule_hours, default_schedule, sizeof(default_schedule));
            cfg.schedule_count = sizeof(default_schedule) / sizeof(int);
        }
        prefs.end();
    }
    void save(const DeviceConfig& cfg) {
        Preferences prefs;
        prefs.begin("jamur-config", false);
        prefs.putFloat("h_crit", cfg.humidity_critical);
        prefs.putFloat("h_warn", cfg.humidity_warning);
        prefs.putBytes("schedules", cfg.schedule_hours, sizeof(int) * cfg.schedule_count);
        prefs.end();
    }
};
ConfigStorage configStorage;

// =================================================================
//   FUNGSI SETUP UTAMA
// =================================================================
void setup() {
    Serial.begin(115200);
    Serial.print("\n\n=== Jamur IoT ");
    Serial.print(FIRMWARE_VERSION);
    Serial.println(" Booting... ===");
    init_hardware();
    load_config();
    init_storage_and_wifi();
}

// =================================================================
//   FUNGSI LOOP UTAMA (State Machine)
// =================================================================
void loop() {
    check_buttons();
    switch (currentState) {
        case STATE_AP_MODE:
            server.handleClient();
            break;
        case STATE_CONNECTING:
            handle_connecting_state();
            break;
        case STATE_NORMAL_OPERATION:
            try_reconnect_mqtt();
            mqttClient.loop();
            handle_main_logic();
            display_normal_info();
            break;
        case STATE_MENU_INFO:
            // Tampilan sudah diatur oleh check_buttons, loop hanya menunggu input
            break;
        case STATE_UPDATING:
             delay(1000);
            break;
    }
}

// =================================================================
// =================================================================
// ==              IMPLEMENTASI SEMUA FUNGSI                    ==
// =================================================================
// =================================================================


// =================================================================
//   Fungsi-fungsi Inisialisasi & Konfigurasi
// =================================================================
void init_hardware() {
    Serial.println("Inisialisasi Hardware...");
    pinMode(PUMP_RELAY_PIN, OUTPUT);
    digitalWrite(PUMP_RELAY_PIN, LOW);
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_OK_PIN, INPUT_PULLUP);
    pinMode(BTN_BACK_PIN, INPUT_PULLUP);
    dht.begin();
    lcd.init();
    lcd.backlight();
    display_boot_screen();
}

void load_config() {
    Serial.println("Memuat konfigurasi...");
    configStorage.load(config);
    Serial.println("Konfigurasi dimuat.");
}

void save_config() {
    Serial.println("Menyimpan konfigurasi...");
    configStorage.save(config);
    Serial.println("Konfigurasi tersimpan.");
}

void init_storage_and_wifi() {
    Serial.println("Inisialisasi Penyimpanan dan WiFi...");
    preferences.begin("jamur-app", false);
    
    WiFi.mode(WIFI_AP_STA);
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    snprintf(mqttClientId, sizeof(mqttClientId), "%s%s", MQTT_CLIENT_ID_PREFIX, mac.substring(6).c_str());
    Serial.printf("MQTT Client ID: %s\n", mqttClientId);
    
    Serial.println("Mengecek tombol BACK untuk masuk mode AP (tahan saat boot)...");
    delay(1000); 
    if (digitalRead(BTN_BACK_PIN) == LOW) {
        currentState = STATE_AP_MODE;
        start_ap_mode();
    } else {
        preferences.getString("wifi_ssid", WIFI_SSID, sizeof(WIFI_SSID));
        if (strcmp(WIFI_SSID, "DEFAULT_SSID") == 0) {
            currentState = STATE_AP_MODE;
            start_ap_mode();
        } else {
            preferences.getString("wifi_pass", WIFI_PASSWORD, sizeof(WIFI_PASSWORD));
            currentState = STATE_CONNECTING;
        }
    }
    preferences.end();
}

void init_mqtt() {
    Serial.println("Mengatur waktu dari server NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);

    struct tm timeinfo;
    // Tunggu hingga waktu berhasil didapatkan (tahun lebih besar dari 2022)
    while (!getLocalTime(&timeinfo) || timeinfo.tm_year < (2022 - 1900)) {
        Serial.println("Gagal mendapatkan waktu, mencoba lagi dalam 1 detik...");
        delay(1000);
    }
    Serial.println("Waktu berhasil disinkronkan.");
    
    Serial.println("Mengatur koneksi aman (TLS)...");
    espClient.setCACert(HIVE_MQ_ROOT_CA);
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqtt_callback);
}

// --- Handler Mode Operasi & Web Server ---
void handle_connecting_state() {
    display_connecting_wifi();
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int attempt = 0; attempt < 20 && WiFi.status() != WL_CONNECTED; attempt++) {
        delay(500);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi terhubung!");
        init_mqtt();
        currentState = STATE_NORMAL_OPERATION;
    } else {
        lcd.clear();
        lcd.print("Koneksi Gagal!");
        lcd.setCursor(0, 1);
        lcd.print("Tahan BACK u/AP");
        delay(5000);
        ESP.restart();
    }
}

void handle_normal_operation() {
    if (!mqttClient.connected()) reconnect_mqtt();
    mqttClient.loop();
    handle_main_logic();
    display_normal_info();
}

void start_ap_mode() {
    Serial.println("Memulai Mode Access Point (AP)...");
    WiFi.softAP(AP_SSID, AP_PASSWORD);
    IPAddress IP = WiFi.softAPIP();
    display_ap_info(IP);
    Serial.printf("Hubungkan ke WiFi '%s' dan buka http://%s\n", AP_SSID, IP.toString().c_str());
    server.on("/", HTTP_GET, handle_web_root);
    server.on("/save", HTTP_POST, handle_web_save);
    server.begin();
    Serial.println("Web server dimulai.");
}

void handle_web_root() {
    String html = "<html><head><title>Jamur IoT Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'></head>";
    html += "<style>body{font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4;}";
    html += "div{background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}";
    html += "input{width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border-radius: 4px; border: 1px solid #ccc;}";
    html += "button{padding: 10px 20px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;}</style>";
    html += "<body><div><h1>Konfigurasi WiFi Jamur IoT</h1>";
    html += "<form action='/save' method='post'>";
    html += "<input type='text' name='ssid' placeholder='Nama WiFi (SSID)' required><br>";
    html += "<input type='password' name='pass' placeholder='Password WiFi'><br>";
    html += "<button type='submit'>Simpan & Reboot</button>";
    html += "</form></div></body></html>";
    server.send(200, "text/html", html);
}

void handle_web_save() {
    String new_ssid = server.arg("ssid");
    String new_pass = server.arg("pass");
    if (new_ssid.length() == 0) {
        server.send(400, "text/html", "<h1>Gagal!</h1><p>SSID tidak boleh kosong.</p>");
        return;
    }
    if (new_ssid.length() > 32) {
        server.send(400, "text/html", "<h1>Gagal!</h1><p>SSID terlalu panjang (maks 32 karakter).</p>");
        return;
    }
    if (new_pass.length() > 64) {
        server.send(400, "text/html", "<h1>Gagal!</h1><p>Password terlalu panjang (maks 64 karakter).</p>");
        return;
    }
    lcd.clear();
    lcd.print("Menyimpan Data..");
    Preferences prefs;
    prefs.begin("jamur-app", false);
    prefs.putString("wifi_ssid", new_ssid);
    prefs.putString("wifi_pass", new_pass);
    prefs.end();
    Serial.printf("Kredensial baru disimpan: SSID=%s\n", new_ssid.c_str());
    server.send(200, "text/html", "<h1>Data Tersimpan!</h1><p>Perangkat akan restart dalam 5 detik...</p>");
    delay(5000);
    ESP.restart();
}

// --- Logika Utama & Tampilan ---
void handle_main_logic() {
    if (millis() - lastLogicCheckTime >= LOGIC_CHECK_INTERVAL_MS) {
        lastLogicCheckTime = millis();
        currentHumidity = dht.readHumidity();
        currentTemperature = dht.readTemperature();
        if (isnan(currentHumidity) || isnan(currentTemperature)) return;
        publish_telemetry();
        run_humidity_control_logic(currentHumidity);
        run_scheduled_control(currentHumidity);
    }
    if (millis() - lastWifiSignalPublishTime >= WIFI_SIGNAL_PUBLISH_INTERVAL_MS) {
        lastWifiSignalPublishTime = millis();
        publish_wifi_signal();
    }
    if (isPumpOn && millis() >= pumpStopTime) {
        turn_pump_off();
    }
    // Notifikasi periodik setiap 1 menit
    if (millis() - lastPeriodicNotifTime >= NOTIF_PERIODIC_INTERVAL_MS) {
        lastPeriodicNotifTime = millis();
        char msg[128];
        snprintf(msg, sizeof(msg), "Periodic status: H=%.1f%%, T=%.1fC, Pump=%s", currentHumidity, currentTemperature, isPumpOn ? "ON" : "OFF");
        send_notification("info", msg, currentHumidity, currentTemperature);
    }
}

void display_boot_screen() {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Jamur IoT V21");
    lcd.setCursor(0, 1); lcd.print("Booting...");
}

void display_connecting_wifi() {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Hubungkan WiFi:");
    lcd.setCursor(0, 1); lcd.print(WIFI_SSID);
}

void display_ap_info(IPAddress ip) {
    lcd.clear();
    lcd.print("Mode Setup WiFi");
    lcd.setCursor(0, 1);
    char ipline[17];
    snprintf(ipline, sizeof(ipline), "IP:%s", ip.toString().c_str());
    lcd.print(ipline);
}

void display_normal_info() {
    static unsigned long lastDisplayTime = 0;
    if (millis() - lastDisplayTime < 1000 && !okButtonPressed) return;
    lastDisplayTime = millis();
    okButtonPressed = false;

    lcd.clear();
    lcd.setCursor(0, 0);
    char line1[17];
    snprintf(line1, sizeof(line1), "H:%.1f%% T:%.1fC", currentHumidity, currentTemperature);
    lcd.print(line1);

    lcd.setCursor(0, 1);
    char line2[17];
    const char* pumpStatusStr = isPumpOn ? "ON " : "OFF";
    snprintf(line2, sizeof(line2), "P:%s MQTT:%s", pumpStatusStr, mqttClient.connected() ? "OK" : "ERR");
    lcd.print(line2);
}

void display_menu_info() {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("INFO PERANGKAT");
    lcd.setCursor(0, 1);
    char ipline[17];
    snprintf(ipline, sizeof(ipline), "IP:%s", WiFi.localIP().toString().c_str());
    lcd.print(ipline);
}

// --- Input Tombol ---
void check_buttons() {
    if (digitalRead(BTN_OK_PIN) == LOW) {
        if (millis() - lastOkDebounceTime > DEBOUNCE_DELAY_MS) {
            if (btnOkPressTime == 0) {
                btnOkPressTime = millis();
            } else if (millis() - btnOkPressTime > LONG_PRESS_MS && !okButtonLongPress) {
                Serial.println("Tombol OK ditekan lama -> Siram Manual");
                turn_pump_on("manual_fisik");
                okButtonLongPress = true;
            }
        }
        lastOkDebounceTime = millis();
    } else {
        if (btnOkPressTime > 0 && !okButtonLongPress) {
            Serial.println("Tombol OK ditekan singkat -> Buka Menu");
             if (currentState == STATE_NORMAL_OPERATION) {
                currentState = STATE_MENU_INFO;
                display_menu_info();
            }
        }
        btnOkPressTime = 0;
        okButtonLongPress = false;
    }

    if (digitalRead(BTN_BACK_PIN) == LOW) {
        static unsigned long lastDebounceTime = 0;
        if (millis() - lastDebounceTime > DEBOUNCE_DELAY_MS) {
            Serial.println("Tombol KEMBALI ditekan");
            if (currentState == STATE_MENU_INFO) {
                currentState = STATE_NORMAL_OPERATION;
            }
        }
        lastDebounceTime = millis();
    }
}

// --- MQTT & Komunikasi ---
// Non-blocking MQTT reconnect
void try_reconnect_mqtt() {
    if (!mqttClient.connected() && millis() - lastMqttRetryTime > MQTT_RETRY_INTERVAL) {
        lastMqttRetryTime = millis();
        Serial.print("Mencoba koneksi MQTT (aman)...");
        if (mqttClient.connect(mqttClientId, MQTT_USER, MQTT_PASSWORD, TOPICS.status, 1, true, "{\"state\":\"offline\"}")) {
            Serial.println("terhubung!");
            mqttClient.publish(TOPICS.status, "{\"state\":\"online\"}");
            mqttClient.subscribe(TOPICS.pump_control);
            mqttClient.subscribe(TOPICS.config_set);
            mqttClient.subscribe(TOPICS.system_update);
            publish_config();
            publish_current_version();
            Serial.println("Berlangganan topik MQTT berhasil.");
        } else {
            Serial.printf("gagal, rc=%d. ", mqttClient.state());
            char error_buf[100];
            espClient.lastError(error_buf, sizeof(error_buf));
            Serial.printf("Keterangan: %s\n", error_buf);
        }
    }
}

void mqtt_callback(char* topic, byte* payload, unsigned int length) {
    payload[length] = '\0';
    Serial.printf("Pesan diterima [%s]: %s\n", topic, (char*)payload);
    if (strcmp(topic, TOPICS.pump_control) == 0 && strcmp((char*)payload, "ON") == 0) {
        turn_pump_on("manual_mqtt");
    } else if (strcmp(topic, TOPICS.config_set) == 0) {
        handle_config_update(payload, length);
    } else if (strcmp(topic, TOPICS.system_update) == 0) {
        JsonDocument doc; // Gunakan JsonDocument untuk v7+
        deserializeJson(doc, payload, length);
        if (!doc["command"].isNull() && doc["command"] == "FIRMWARE_UPDATE") {
            String url = doc["url"];
            if (url.length() > 0) {
                perform_ota_update(url);
            }
        }
    }
}

void handle_config_update(byte* payload, unsigned int length) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error) {
        Serial.printf("deserializeJson() gagal: %s\n", error.c_str());
        return;
    }
    Serial.println("Menerima pembaruan konfigurasi dari MQTT.");
    config.humidity_critical = doc["h_crit"] | config.humidity_critical;
    config.humidity_warning = doc["h_warn"] | config.humidity_warning;
    if (doc["schedules"].is<JsonArray>()) {
        JsonArray newSchedules = doc["schedules"].as<JsonArray>();
        int validCount = 0;
        for (int i = 0; i < newSchedules.size() && validCount < 5; i++) {
            int jam = newSchedules[i];
            if (jam >= 0 && jam <= 23) {
                config.schedule_hours[validCount++] = jam;
            } else {
                Serial.printf("Jadwal jam tidak valid: %d (abaikan)\n", jam);
            }
        }
        config.schedule_count = validCount;
    }
    save_config();
    publish_config();
}

void publish_telemetry() {
    char payload[100];
    snprintf(payload, sizeof(payload), "{\"temperature\":%.2f, \"humidity\":%.2f}", currentTemperature, currentHumidity);
    mqttClient.publish(TOPICS.telemetry, payload);
}

void publish_wifi_signal() {
    long rssi = WiFi.RSSI();
    char payload[50];
    snprintf(payload, sizeof(payload), "{\"rssi\":%ld}", rssi);
    mqttClient.publish(TOPICS.wifi_signal, payload, true);
}

void publish_config() {
    JsonDocument doc;
    doc["h_crit"] = config.humidity_critical;
    doc["h_warn"] = config.humidity_warning;
    JsonArray schedules = doc["schedules"].to<JsonArray>();
    for (int i = 0; i < config.schedule_count; i++) {
        schedules.add(config.schedule_hours[i]);
    }
    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPICS.config_get, buffer, true);
}

/**
 * @brief Mempublikasikan versi firmware saat ini ke MQTT.
 */
void publish_current_version() {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    char payload[50];
    serializeJson(doc, payload);
    // Kirim dengan flag retained agar frontend selalu tahu versi terakhir
    mqttClient.publish(TOPICS.firmware_current, payload, true); 
    Serial.printf("Versi firmware saat ini (%s) dipublikasikan.\n", FIRMWARE_VERSION);
}

// --- Fungsi Publish Status Firmware ---
/**
 * @brief Publish status update firmware ke MQTT.
 * @param status Status update: "updating", "updated", atau "failed"
 */
void publish_firmware_status(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    doc["version"] = FIRMWARE_VERSION;
    char payload[64];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_current, payload, true);
}


// --- Logika Kontrol & Aksi ---
void run_humidity_control_logic(float humidity) {
    if (humidity < config.humidity_critical) {
        turn_pump_on("auto_critical");
        if (lastNotifState != NOTIF_CRITICAL) {
            send_notification("warning", "Humidity below critical threshold! Pump turned ON.", humidity, currentTemperature);
            lastNotifState = NOTIF_CRITICAL;
        }
    } else if (humidity < config.humidity_warning) {
        if (lastNotifState != NOTIF_WARNING) {
            send_notification("warning", "Warning: Humidity approaching lower limit.", humidity, currentTemperature);
            lastNotifState = NOTIF_WARNING;
        }
    } else {
        if (lastNotifState != NOTIF_NORMAL) {
            send_notification("info", "Humidity back to normal.", humidity, currentTemperature);
            lastNotifState = NOTIF_NORMAL;
        }
    }
}

void run_scheduled_control(float humidity) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    int currentHour = timeinfo.tm_hour;
    if (currentHour != lastScheduledHour) {
        for (int i = 0; i < config.schedule_count; i++) {
            if (currentHour == config.schedule_hours[i]) {
                if (humidity >= config.humidity_critical) {
                    turn_pump_on("scheduled");
                    send_notification("info", "Scheduled watering executed.", humidity, currentTemperature);
                } else {
                    char msg[128];
                    snprintf(msg, sizeof(msg), "Scheduled watering at %d:00 skipped, humidity critical (%.1f%%).", currentHour, humidity);
                    send_notification("info", msg, humidity, currentTemperature);
                }
                lastScheduledHour = currentHour;
                break;
            }
        }
    }
}

void turn_pump_on(const char* reason) {
    if (isPumpOn) return;
    isPumpOn = true;
    pumpStopTime = millis() + PUMP_DURATION_MS;
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    mqttClient.publish(TOPICS.status, "{\"state\":\"pumping\"}");
    char msg[128];
    snprintf(msg, sizeof(msg), "Pump turned ON (%s).", reason);
    send_notification("info", msg, currentHumidity, currentTemperature);
}

void turn_pump_off() {
    if (!isPumpOn) return;
    isPumpOn = false;
    digitalWrite(PUMP_RELAY_PIN, LOW);
    mqttClient.publish(TOPICS.status, "{\"state\":\"idle\"}");
    send_notification("info", "Pump turned OFF.", currentHumidity, currentTemperature);
}

// --- Fungsi OTA Update ---
void perform_ota_update(String url) {
    Serial.printf("Received OTA Update command. URL: %s\n", url.c_str());
    currentState = STATE_UPDATING;
    publish_firmware_status("updating");
    lcd.clear();
    lcd.print("Firmware Update");
    lcd.setCursor(0, 1);
    lcd.print("Downloading...");
    HTTPClient http;
    http.begin(url);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
        int contentLength = http.getSize();
        if (contentLength > 0) {
            if ((uint32_t)ESP.getFreeSketchSpace() < (uint32_t)contentLength) {
                Serial.println("Not enough free space for OTA update.");
                lcd.clear();
                lcd.print("OTA: No space!");
                send_notification("error", "OTA update failed: not enough space.");
                http.end();
                delay(5000);
                currentState = STATE_NORMAL_OPERATION;
                return;
            }
            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                WiFiClient& stream = http.getStream();
                size_t written = Update.writeStream(stream);
                if (written == contentLength) {
                    Serial.println("Firmware download successful.");
                } else {
                    Serial.printf("Firmware download failed, size mismatch. Written: %d, Expected: %d\n", written, contentLength);
                }
                if (Update.end()) {
                    if (Update.isFinished()) {
                        Serial.println("Update successful! Restarting...");
                        lcd.clear();
                        lcd.print("Update Success!");
                        lcd.setCursor(0, 1);
                        lcd.print("Restarting...");
                        publish_firmware_status("updated");
                        send_notification("info", "Firmware updated successfully.");
                        delay(2000);
                        ESP.restart();
                    } else {
                        Serial.println("Failed to finish update.");
                    }
                } else {
                    Serial.printf("OTA Error: #%u\n", Update.getError());
                }
            } else {
                Serial.println("Not enough memory for OTA.");
            }
        } else {
            Serial.println("Unknown content length.");
        }
    } else {
        Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
    }
    http.end();
    lcd.clear();
    lcd.print("Update Failed!");
    lcd.setCursor(0, 1);
    lcd.print("Check Serial Mon.");
    publish_firmware_status("failed");
    send_notification("error", "Firmware update failed.");
    delay(5000);
    currentState = STATE_NORMAL_OPERATION;
}