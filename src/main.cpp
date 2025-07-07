// =================================================================
//   FIRMWARE KONTROL BUDIDAYA JAMUR TIRAM V17.3 (ArduinoJson v7 Fix)
//   Oleh: Google Gemini untuk bagus-erwanto.vercel.app
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

// =================================================================
//   FUNGSI SETUP UTAMA
// =================================================================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== Jamur IoT V20 Booting.. ===");

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
            if (!mqttClient.connected()) reconnect_mqtt();
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
    preferences.begin("jamur-config", true);
    config.humidity_critical = preferences.getFloat("h_crit", 80.0);
    config.humidity_warning = preferences.getFloat("h_warn", 85.0);
    config.schedule_count = preferences.getBytes("schedules", config.schedule_hours, sizeof(config.schedule_hours)) / sizeof(int);
    if (config.schedule_count == 0) {
        Serial.println("Tidak ada jadwal tersimpan, menggunakan default.");
        int default_schedule[] = {7, 12, 17};
        memcpy(config.schedule_hours, default_schedule, sizeof(default_schedule));
        config.schedule_count = sizeof(default_schedule) / sizeof(int);
    }
    preferences.end();
    Serial.println("Konfigurasi dimuat.");
}

void save_config() {
    Serial.println("Menyimpan konfigurasi...");
    preferences.begin("jamur-config", false);
    preferences.putFloat("h_crit", config.humidity_critical);
    preferences.putFloat("h_warn", config.humidity_warning);
    preferences.putBytes("schedules", config.schedule_hours, sizeof(int) * config.schedule_count);
    preferences.end();
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
    if (new_ssid.length() > 0) {
        lcd.clear();
        lcd.print("Menyimpan Data..");
        preferences.begin("jamur-app", false);
        preferences.putString("wifi_ssid", new_ssid);
        preferences.putString("wifi_pass", new_pass);
        preferences.end();
        Serial.printf("Kredensial baru disimpan: SSID=%s\n", new_ssid.c_str());
        server.send(200, "text/html", "<h1>Data Tersimpan!</h1><p>Perangkat akan restart dalam 5 detik...</p>");
        delay(5000);
        ESP.restart();
    } else {
        server.send(400, "text/html", "<h1>Gagal!</h1><p>SSID tidak boleh kosong.</p>");
    }
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
}

void display_boot_screen() {
    lcd.clear();
    lcd.setCursor(0, 0); lcd.print("Jamur IoT V17.3");
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
        if (btnOkPressTime == 0) {
            btnOkPressTime = millis();
        } else if (millis() - btnOkPressTime > LONG_PRESS_MS && !okButtonLongPress) {
            Serial.println("Tombol OK ditekan lama -> Siram Manual");
            turn_pump_on("manual_fisik");
            okButtonLongPress = true;
        }
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
void reconnect_mqtt() {
    while (!mqttClient.connected()) {
        Serial.print("Mencoba koneksi MQTT (aman)...");
        if (mqttClient.connect(mqttClientId, MQTT_USER, MQTT_PASSWORD, TOPICS.status, 1, true, "{\"state\":\"offline\"}")) {
            Serial.println("terhubung!");
            mqttClient.publish(TOPICS.status, "{\"state\":\"online\"}");
            mqttClient.subscribe(TOPICS.pump_control);
            mqttClient.subscribe(TOPICS.config_set);
            mqttClient.subscribe(TOPICS.system_update);
            publish_config();
        } else {
            Serial.printf("gagal, rc=%d. ", mqttClient.state());
            // Mencetak detail error dari lapisan SSL/TLS
            char error_buf[100];
            espClient.lastError(error_buf, sizeof(error_buf));
            Serial.printf("Keterangan: %s\n", error_buf);

            Serial.println("Coba lagi dalam 5 detik...");
            delay(5000);
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
    // <<< PERBAIKAN ARDUINOJSON V7 >>>
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload, length);

    if (error) {
        Serial.printf("deserializeJson() gagal: %s\n", error.c_str());
        return;
    }
    Serial.println("Menerima pembaruan konfigurasi dari MQTT.");
    
    // Gunakan sintaks baru untuk mendapatkan nilai dengan default
    config.humidity_critical = doc["h_crit"] | config.humidity_critical;
    config.humidity_warning = doc["h_warn"] | config.humidity_warning;

    // Gunakan sintaks baru untuk mengecek keberadaan kunci
    if (doc["schedules"].is<JsonArray>()) {
        JsonArray newSchedules = doc["schedules"].as<JsonArray>();
        config.schedule_count = newSchedules.size() < 5 ? newSchedules.size() : 5;
        for(int i=0; i < config.schedule_count; i++) {
            config.schedule_hours[i] = newSchedules[i];
        }
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
    // <<< PERBAIKAN ARDUINOJSON V7 >>>
    JsonDocument doc; // Otomatis mengelola memori
    
    doc["h_crit"] = config.humidity_critical;
    doc["h_warn"] = config.humidity_warning;
    
    // Gunakan sintaks baru untuk membuat array
    JsonArray schedules = doc["schedules"].to<JsonArray>();
    for (int i = 0; i < config.schedule_count; i++) {
        schedules.add(config.schedule_hours[i]);
    }

    char buffer[256];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPICS.config_get, buffer, true);
    Serial.println("Konfigurasi saat ini dipublikasikan ke MQTT.");
}

// --- Logika Kontrol & Aksi ---
void run_humidity_control_logic(float humidity) {
    if (humidity < config.humidity_critical) {
        if (!isPumpOn) turn_pump_on("otomatis_kritis");
        isWarningZone = false;
    } else if (humidity < config.humidity_warning) {
        if (!isWarningZone) {
            char msg[200];
            snprintf(msg, sizeof(msg), "{\"type\":\"warning\", \"message\":\"HATI-HATI: Kelembapan mendekati batas bawah (%.1f%%).\"}", humidity);
            mqttClient.publish(TOPICS.notification, msg);
            isWarningZone = true;
        }
    } else {
        if (isWarningZone) {
            isWarningZone = false;
            char msg[200];
            snprintf(msg, sizeof(msg), "{\"type\":\"info\", \"message\":\"Kelembapan kembali normal (%.1f%%).\"}", humidity);
            mqttClient.publish(TOPICS.notification, msg);
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
                    turn_pump_on("terjadwal");
                } else {
                    char msg[200];
                    snprintf(msg, sizeof(msg), "{\"type\":\"info\", \"message\":\"Jadwal jam %d:00 dilewati, kondisi kritis (%.1f%%).\"}", currentHour, humidity);
                    mqttClient.publish(TOPICS.notification, msg);
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
    char notifPayload[200];
    snprintf(notifPayload, sizeof(notifPayload), "{\"type\":\"info\", \"message\":\"Pompa dinyalakan (%s).\", \"humidity\":%.1f, \"temperature\":%.1f}", reason, currentHumidity, currentTemperature);
    mqttClient.publish(TOPICS.notification, notifPayload);
}

void turn_pump_off() {
    if (!isPumpOn) return;
    isPumpOn = false;
    digitalWrite(PUMP_RELAY_PIN, LOW);
    mqttClient.publish(TOPICS.status, "{\"state\":\"idle\"}");
    char notifPayload[200];
    snprintf(notifPayload, sizeof(notifPayload), "{\"type\":\"info\", \"message\":\"Pompa telah berhenti.\", \"humidity\":%.1f, \"temperature\":%.1f}", currentHumidity, currentTemperature);
    mqttClient.publish(TOPICS.notification, notifPayload);
}

// --- Fungsi OTA Update ---
void perform_ota_update(String url) {
    Serial.printf("Menerima perintah OTA Update. URL: %s\n", url.c_str());
    currentState = STATE_UPDATING;

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
            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                WiFiClient& stream = http.getStream();
                size_t written = Update.writeStream(stream);
                if (written == contentLength) {
                    Serial.println("Download firmware berhasil.");
                } else {
                    Serial.printf("Download firmware gagal, ukuran tidak cocok. Tertulis: %d, Seharusnya: %d\n", written, contentLength);
                }
                if (Update.end()) {
                    if (Update.isFinished()) {
                        Serial.println("Update berhasil! Me-restart...");
                        lcd.clear();
                        lcd.print("Update Sukses!");
                        lcd.setCursor(0, 1);
                        lcd.print("Restarting...");
                        delay(2000);
                        ESP.restart();
                    } else {
                        Serial.println("Gagal menyelesaikan update.");
                    }
                } else {
                    Serial.printf("Error OTA: #%u\n", Update.getError());
                }
            } else {
                Serial.println("Memori tidak cukup untuk memulai OTA.");
            }
        } else {
            Serial.println("Ukuran konten tidak diketahui.");
        }
    } else {
        Serial.printf("HTTP GET gagal, kode error: %d\n", httpCode);
    }
    http.end();
    
    // Jika sampai di sini, berarti update gagal
    lcd.clear();
    lcd.print("Update Gagal!");
    lcd.setCursor(0, 1);
    lcd.print("Cek Serial Mon.");
    delay(5000);
    currentState = STATE_NORMAL_OPERATION;
}