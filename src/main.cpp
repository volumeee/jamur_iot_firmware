// =================================================================
//   FIRMWARE KONTROL BUDIDAYA JAMUR TIRAM V17.3 (ArduinoJson v7 Fix)
//   Oleh: bagus-erwanto.vercel.app
//   Tanggal: 6 Juli 2025
// =================================================================

// --- Library & File Pendukung (urutan penting) ---
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
#include <WiFiClient.h>

#include "config.h"
#include "functions.h"

// =================================================================
//   Deklarasi objek & variabel global
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
char mqttClientId[MQTT_CLIENT_ID_LENGTH];

unsigned long lastLogicCheckTime = 0, lastWifiSignalPublishTime = 0, pumpStopTime = 0;
int lastScheduledHour = -1;
unsigned long btnOkPressTime = 0;
bool okButtonLongPress = false;
bool okButtonPressed = false;
unsigned long lastPeriodicNotifTime = 0;
unsigned long lastOkDebounceTime = 0;
unsigned long lastMqttRetryTime = 0;
FirmwareInfo newFirmware;
enum NotifState { NOTIF_NORMAL, NOTIF_WARNING, NOTIF_CRITICAL };
NotifState lastNotifState = NOTIF_NORMAL;
unsigned long lastSpeedtestTime = 0;
unsigned long lastWifiReconnectTime = 0;
const unsigned long WIFI_RECONNECT_INTERVAL = WIFI_RECONNECT_DELAY; // 10 detik
unsigned long pumpCountdownLastPublish = 0;
int pumpCountdownSeconds = 0;

// Fungsi untuk cek dan reconnect WiFi jika terputus
void check_and_reconnect_wifi() {
    if (WiFi.status() != WL_CONNECTED && millis() - lastWifiReconnectTime > WIFI_RECONNECT_INTERVAL) {
        Serial.println("WiFi terputus! Mencoba reconnect...");
        lcd_show_message("WiFi Terputus!", "Reconnect...");
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        lastWifiReconnectTime = millis();
    }
}

// Publish MQTT dengan retry
bool publish_with_retry(const char* topic, const char* payload, bool retained = false, int retry = NOTIF_RETRY_COUNT, int delayMs = NOTIF_RETRY_DELAY_MS) {
    for (int i = 0; i < retry; ++i) {
        if (mqttClient.publish(topic, payload, retained)) {
            return true;
        }
        delay(delayMs);
    }
    return false;
}

// Kirim notifikasi dalam format JSON
void send_notification(const char* type, const char* message, float humidity = -1, float temperature = -1) {
    char notifPayload[NOTIF_PAYLOAD_SIZE];
    if (humidity >= 0 && temperature >= 0) {
        snprintf(notifPayload, NOTIF_PAYLOAD_SIZE,
            "{\"type\":\"%s\", \"message\":\"%s\", \"humidity\":%.1f, \"temperature\":%.1f}",
            type, message, humidity, temperature);
    } else {
        snprintf(notifPayload, NOTIF_PAYLOAD_SIZE,
            "{\"type\":\"%s\", \"message\":\"%s\"}", type, message);
    }
    publish_with_retry(TOPICS.notification, notifPayload);
}

// Penyimpanan konfigurasi ke Preferences
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
//   FUNGSI INISIALISASI & KONFIGURASI
// =================================================================
void init_hardware();
void load_config();
void save_config();
void init_storage_and_wifi();
void init_mqtt();

// =================================================================
//   FUNGSI SETUP & LOOP UTAMA
// =================================================================
void setup();
void loop();

// =================================================================
//   FUNGSI HANDLER MODE OPERASI & WEB SERVER
// =================================================================
void handle_connecting_state();
void handle_normal_operation();
void start_ap_mode();
void handle_web_root();
void handle_web_save();

// =================================================================
//   FUNGSI TAMPILAN LCD
// =================================================================
void display_boot_screen();
void display_connecting_wifi();
void display_wifi_failed();
void display_ap_info(IPAddress ip);
void display_normal_info();
void display_menu_info();

// =================================================================
//   FUNGSI INPUT TOMBOL
// =================================================================
void check_buttons();

// =================================================================
//   FUNGSI LOGIKA UTAMA & KONTROL
// =================================================================
void handle_main_logic();
void run_humidity_control_logic(float humidity);
void run_scheduled_control(float humidity);
void turn_pump_on(const char* reason);
void turn_pump_off();

// Fungsi untuk publish countdown ke MQTT
void publish_pump_countdown(int seconds) {
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"countdown\":%d}", seconds);
    mqttClient.publish(TOPICS.pump_countdown, payload, true); // Retained agar client tahu status terakhir
}

// =================================================================
//   FUNGSI KOMUNIKASI, MQTT, NOTIFIKASI, UTILITAS
// =================================================================
void try_reconnect_mqtt();
void mqtt_callback(char* topic, byte* payload, unsigned int length);
void handle_config_update(byte* payload, unsigned int length);
void publish_telemetry();
void publish_wifi_signal();
void publish_config();
void publish_current_version();
void publish_firmware_status(const char* status);
void publish_firmware_update_progress(const char* stage, int progress, const char* message);
void trigger_email_notification(const NotificationData& data);
void check_for_firmware_update();

// =================================================================
//   FUNGSI OTA UPDATE
// =================================================================
void perform_ota_update(String url);

// =================================================================
//   FUNGSI UTILITAS
// =================================================================
void lcd_show_message(const char* line1, const char* line2);
void pause_and_restart(unsigned long ms);

// =================================================================
//   IMPLEMENTASI FUNGSI
// =================================================================
//
// =============================
// == INISIALISASI & KONFIG  ==
// =============================
void init_hardware() {
    Serial.println("Inisialisasi hardware...");
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
    Serial.println("Inisialisasi penyimpanan dan WiFi...");
    preferences.begin("jamur-app", false);
    WiFi.mode(WIFI_AP_STA);
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    snprintf(mqttClientId, MQTT_CLIENT_ID_LENGTH, "%s%s", MQTT_CLIENT_ID_PREFIX, mac.substring(6).c_str());
    Serial.printf("MQTT Client ID: %s\n", mqttClientId);
    Serial.println("Cek tombol BACK untuk mode AP (tahan saat boot)...");
    delay(NTP_RETRY_DELAY); 
    if (digitalRead(BTN_BACK_PIN) == LOW) {
        currentState = STATE_AP_MODE;
        start_ap_mode();
    } else {
        String stored_ssid = preferences.getString("wifi_ssid", "");
        String stored_pass = preferences.getString("wifi_pass", "");
        if (stored_ssid.length() == 0) {
            Serial.println("SSID tidak ditemukan di Preferences, gunakan default dari secrets.h");
            strncpy(WIFI_SSID, SECRET_WIFI_SSID, sizeof(WIFI_SSID) - 1);
            WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
            strncpy(WIFI_PASSWORD, SECRET_WIFI_PASS, sizeof(WIFI_PASSWORD) - 1);
            WIFI_PASSWORD[sizeof(WIFI_PASSWORD) - 1] = '\0';
            Serial.printf("Menggunakan SSID default: %s\n", WIFI_SSID);
            if (strlen(WIFI_SSID) == 0) {
                Serial.println("Default SSID kosong, masuk mode AP");
                currentState = STATE_AP_MODE;
                start_ap_mode();
            } else {
                currentState = STATE_CONNECTING;
            }
        } else {
            strncpy(WIFI_SSID, stored_ssid.c_str(), sizeof(WIFI_SSID) - 1);
            WIFI_SSID[sizeof(WIFI_SSID) - 1] = '\0';
            strncpy(WIFI_PASSWORD, stored_pass.c_str(), sizeof(WIFI_PASSWORD) - 1);
            WIFI_PASSWORD[sizeof(WIFI_PASSWORD) - 1] = '\0';
            Serial.printf("Menggunakan SSID dari Preferences: %s\n", WIFI_SSID);
            currentState = STATE_CONNECTING;
        }
    }
    preferences.end();
}

void init_mqtt() {
    Serial.println("Sinkronisasi waktu NTP...");
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo) || timeinfo.tm_year < (2022 - 1900)) {
        Serial.println("Gagal mendapatkan waktu, retry 1 detik...");
        delay(NTP_RETRY_DELAY);
    }
    Serial.println("Waktu tersinkron.");
    Serial.println("Setup koneksi TLS...");
    espClient.setCACert(HIVE_MQ_ROOT_CA);
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setKeepAlive(MQTT_KEEP_ALIVE_SEC);
    mqttClient.setCallback(mqtt_callback);
}

// =============================
// == SETUP & LOOP UTAMA      ==
// =============================
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
//   Loop utama (state machine)
// =================================================================
void loop() {
    check_buttons();
    // Publish countdown setiap detik saat pompa ON
    if (isPumpOn) {
        unsigned long now = millis();
        int secondsLeft = (pumpStopTime > now) ? (int)((pumpStopTime - now) / 1000) : 0;
        if (secondsLeft < 0) secondsLeft = 0;
        if (secondsLeft != pumpCountdownSeconds || now - pumpCountdownLastPublish >= 1000) {
            pumpCountdownSeconds = secondsLeft;
            publish_pump_countdown(pumpCountdownSeconds);
            pumpCountdownLastPublish = now;
        }
    }
    switch (currentState) {
        case STATE_AP_MODE:
            server.handleClient();
            break;
        case STATE_CONNECTING:
            handle_connecting_state();
            break;
        case STATE_NORMAL_OPERATION:
            check_and_reconnect_wifi();
            try_reconnect_mqtt();
            mqttClient.loop();
            handle_main_logic();
            display_normal_info();
            break;
        case STATE_MENU_INFO:
            break;
        case STATE_UPDATING:
            delay(1000);
            break;
    }
}

// =============================
// == HANDLER MODE/WEB SERVER ==
// =============================
void handle_connecting_state() {
    display_connecting_wifi();
    Serial.printf("Mencoba koneksi ke WiFi: %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    for (int attempt = 0; attempt < WIFI_CONNECT_ATTEMPTS && WiFi.status() != WL_CONNECTED; attempt++) {
        delay(WIFI_CONNECT_DELAY);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi terhubung!");
        Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
        init_mqtt();
        currentState = STATE_NORMAL_OPERATION;
    } else {
        Serial.printf("\nKoneksi WiFi gagal. Status: %d\n", WiFi.status());
        Serial.println("Masuk ke mode AP untuk konfigurasi WiFi...");
        display_wifi_failed();
        currentState = STATE_AP_MODE;
        start_ap_mode();
    }
}

void handle_normal_operation() {
    if (!mqttClient.connected() && millis() - lastMqttRetryTime > MQTT_RETRY_INTERVAL) {
        lastMqttRetryTime = millis();
        Serial.print("Mencoba koneksi MQTT (TLS)...");
        if (mqttClient.connect(
                mqttClientId,
                MQTT_USER,
                MQTT_PASSWORD,
                TOPICS.status, // LWT topic
                1,             // QoS
                true,          // retained
                "{\"state\":\"offline\"}" // LWT payload
            )) {
            Serial.println("terhubung!");
            mqttClient.publish(TOPICS.status, "{\"state\":\"online\"}", true); // Retained!
            mqttClient.subscribe(TOPICS.pump_control);
            mqttClient.subscribe(TOPICS.config_set);
            mqttClient.subscribe(TOPICS.system_update);
            publish_config();
            publish_current_version();
            Serial.println("Berlangganan topik MQTT sukses.");
        } else {
            Serial.printf("gagal, rc=%d. ", mqttClient.state());
            char error_buf[100];
            espClient.lastError(error_buf, sizeof(error_buf));
            Serial.printf("Keterangan: %s\n", error_buf);
        }
    }
    mqttClient.loop();
    handle_main_logic();
    display_normal_info();
}

void start_ap_mode() {
    Serial.println("Mode Access Point (AP)...");
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
    html += "div{background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); margin-bottom: 20px;}";
    html += "input{width: calc(100% - 22px); padding: 10px; margin-bottom: 10px; border-radius: 4px; border: 1px solid #ccc;}";
    html += "button{padding: 10px 20px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer; margin: 5px;}";
    html += ".info{background-color: #e7f3ff; border-left: 4px solid #007bff; padding: 10px; margin: 10px 0; text-align: left;}";
    html += ".warning{background-color: #fff3cd; border-left: 4px solid #ffc107; padding: 10px; margin: 10px 0; text-align: left;}</style>";
    html += "<body><div><h1>🌱 Konfigurasi WiFi Jamur IoT</h1>";
    html += "<div class='info'><strong>Info:</strong> Perangkat tidak dapat terhubung ke WiFi yang tersimpan. Silakan masukkan kredensial WiFi yang benar.</div>";
    html += "<form action='/save' method='post'>";
    html += "<input type='text' name='ssid' placeholder='Nama WiFi (SSID)' required><br>";
    html += "<input type='password' name='pass' placeholder='Password WiFi'><br>";
    html += "<button type='submit'>💾 Simpan & Reboot</button>";
    html += "</form>";
    html += "<div class='warning'><strong>Catatan:</strong> Setelah menyimpan, perangkat akan restart dan mencoba terhubung ke WiFi baru.</div>";
    html += "</div></body></html>";
    server.send(200, "text/html", html);
}

void handle_web_save() {
    String new_ssid = server.arg("ssid");
    String new_pass = server.arg("pass");
    String errorHtml = "<html><head><title>Error - Jamur IoT</title>";
    errorHtml += "<meta name='viewport' content='width=device-width, initial-scale=1'></head>";
    errorHtml += "<style>body{font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4;}";
    errorHtml += "div{background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}";
    errorHtml += ".error{background-color: #f8d7da; border-left: 4px solid #dc3545; padding: 10px; margin: 10px 0; text-align: left;}";
    errorHtml += "button{padding: 10px 20px; background-color: #007bff; color: white; border: none; border-radius: 4px; cursor: pointer;}</style>";
    
    if (new_ssid.length() == 0) {
        errorHtml += "<body><div><h1>❌ Gagal!</h1>";
        errorHtml += "<div class='error'><strong>Error:</strong> SSID tidak boleh kosong.</div>";
        errorHtml += "<button onclick='history.back()'>← Kembali</button></div></body></html>";
        server.send(400, "text/html", errorHtml);
        return;
    }
    if (new_ssid.length() > 32) {
        errorHtml += "<body><div><h1>❌ Gagal!</h1>";
        errorHtml += "<div class='error'><strong>Error:</strong> SSID terlalu panjang (maks 32 karakter).</div>";
        errorHtml += "<button onclick='history.back()'>← Kembali</button></div></body></html>";
        server.send(400, "text/html", errorHtml);
        return;
    }
    if (new_pass.length() > 64) {
        errorHtml += "<body><div><h1>❌ Gagal!</h1>";
        errorHtml += "<div class='error'><strong>Error:</strong> Password terlalu panjang (maks 64 karakter).</div>";
        errorHtml += "<button onclick='history.back()'>← Kembali</button></div></body></html>";
        server.send(400, "text/html", errorHtml);
        return;
    }
    lcd_show_message("Menyimpan Data..", "");
    Preferences prefs;
    prefs.begin("jamur-app", false);
    prefs.putString("wifi_ssid", new_ssid);
    prefs.putString("wifi_pass", new_pass);
    prefs.end();
    Serial.printf("Kredensial baru disimpan: SSID=%s\n", new_ssid.c_str());
    String successHtml = "<html><head><title>Berhasil - Jamur IoT</title>";
    successHtml += "<meta name='viewport' content='width=device-width, initial-scale=1'></head>";
    successHtml += "<style>body{font-family: Arial, sans-serif; text-align: center; margin: 20px; background-color: #f4f4f4;}";
    successHtml += "div{background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);}";
    successHtml += ".success{background-color: #d4edda; border-left: 4px solid #28a745; padding: 10px; margin: 10px 0; text-align: left;}</style>";
    successHtml += "<body><div><h1>✅ Data Tersimpan!</h1>";
    successHtml += "<div class='success'><strong>Berhasil:</strong> Kredensial WiFi baru telah disimpan.</div>";
    successHtml += "<p>Perangkat akan restart dalam 5 detik dan mencoba terhubung ke WiFi baru.</p>";
    successHtml += "<p><small>SSID: " + new_ssid + "</small></p>";
    successHtml += "</div></body></html>";
    server.send(200, "text/html", successHtml);
    pause_and_restart(ERROR_RESTART_DELAY);
}

// =============================
// == TAMPILAN LCD            ==
// =============================
void display_boot_screen() {
    char line1[LCD_LINE_LENGTH];
    snprintf(line1, LCD_LINE_LENGTH, "Jamur IoT %s", FIRMWARE_VERSION);
    lcd_show_message(line1, "Booting...");
}

void display_connecting_wifi() {
    lcd_show_message("Hubungkan WiFi:", WIFI_SSID);
}

void display_wifi_failed() {
    lcd_show_message("WiFi Gagal!", "Masuk Mode AP");
    delay(2000);
    lcd_show_message("Hubungkan ke:", AP_SSID);
    delay(2000);
}

void display_ap_info(IPAddress ip) {
    char ipline[LCD_LINE_LENGTH];
    snprintf(ipline, LCD_LINE_LENGTH, "IP:%s", ip.toString().c_str());
    lcd_show_message("Mode Setup WiFi", ipline);
    
    // Tampilkan informasi tambahan di Serial
    Serial.println("=== MODE ACCESS POINT ===");
    Serial.printf("SSID: %s\n", AP_SSID);
    Serial.printf("Password: %s\n", AP_PASSWORD);
    Serial.printf("IP Address: %s\n", ip.toString().c_str());
    Serial.println("Hubungkan ke WiFi di atas dan buka browser ke IP address tersebut");
    Serial.println("untuk mengkonfigurasi WiFi baru.");
}

void display_normal_info() {
    static unsigned long lastDisplayTime = 0;
    if (millis() - lastDisplayTime < 1000 && !okButtonPressed) return;
    lastDisplayTime = millis();
    okButtonPressed = false;
    lcd.clear();
    lcd.setCursor(0, 0);
    char line1[LCD_LINE_LENGTH];
    snprintf(line1, LCD_LINE_LENGTH, "H:%.1f%% T:%.1fC", currentHumidity, currentTemperature);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    char line2[LCD_LINE_LENGTH];
    const char* pumpStatusStr = isPumpOn ? "ON " : "OFF";
    snprintf(line2, LCD_LINE_LENGTH, "P:%s MQTT:%s", pumpStatusStr, mqttClient.connected() ? "OK" : "ERR");
    lcd.print(line2);
}

void display_menu_info() {
    char ipline[LCD_LINE_LENGTH];
    snprintf(ipline, LCD_LINE_LENGTH, "IP:%s", WiFi.localIP().toString().c_str());
    lcd_show_message("INFO PERANGKAT", ipline);
}

// =============================
// == INPUT TOMBOL            ==
// =============================
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

// =================================================================
//   FUNGSI LOGIKA UTAMA & KONTROL
// =================================================================
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
    if (millis() - lastPeriodicNotifTime >= NOTIF_PERIODIC_INTERVAL_MS) {
        lastPeriodicNotifTime = millis();
        char msg[PERIODIC_MSG_SIZE];
        snprintf(msg, PERIODIC_MSG_SIZE, "Periodic status: H=%.1f%%, T=%.1fC, Pump=%s", currentHumidity, currentTemperature, isPumpOn ? "ON" : "OFF");
        send_notification("info", msg, currentHumidity, currentTemperature);
    }
    long rssi = WiFi.RSSI();
    if ((millis() - lastSpeedtestTime >= SPEEDTEST_INTERVAL_MS) || (rssi < SPEEDTEST_RSSI_THRESHOLD)) {
        lastSpeedtestTime = millis();
        run_and_publish_speedtest();
    }
}

// =============================
// == EMAIL NOTIFICATION RATE LIMIT ==
// =============================
unsigned long lastEmailSent_firmware = 0;
unsigned long lastEmailSent_warning = 0;
unsigned long lastEmailSent_critical = 0;
unsigned long lastEmailSent_normal = 0;


bool can_send_email(unsigned long& lastSent, unsigned long minInterval) {
    unsigned long now = millis();
    if (now < lastSent) lastSent = 0; // handle millis overflow
    if (now - lastSent >= minInterval) {
        lastSent = now;
        return true;
    }
    return false;
}

void run_humidity_control_logic(float humidity) {
    if (humidity < config.humidity_critical) {
        turn_pump_on("auto_critical");
        if (lastNotifState != NOTIF_CRITICAL) {
            send_notification("warning", "Humidity below critical threshold! Pump turned ON.", humidity, currentTemperature);
            NotificationData data;
            data.type = "critical_alert";
            data.message = "Kelembapan di bawah ambang batas kritis!";
            data.humidity = humidity;
            data.temperature = currentTemperature;
            if (can_send_email(lastEmailSent_critical, EMAIL_MIN_INTERVAL_ALERT_MS)) {
                trigger_email_notification(data);
            } else {
                Serial.println("[EMAIL] Critical alert diabaikan (rate limit).");
            }
            lastNotifState = NOTIF_CRITICAL;
        }
    } else if (humidity < config.humidity_warning) {
        if (lastNotifState != NOTIF_WARNING) {
            send_notification("warning", "Warning: Humidity approaching lower limit.", humidity, currentTemperature);
            NotificationData data;
            data.type = "warning";
            data.message = "Kelembapan mendekati ambang batas.";
            data.humidity = humidity;
            data.temperature = currentTemperature;
            if (can_send_email(lastEmailSent_warning, EMAIL_MIN_INTERVAL_ALERT_MS)) {
                trigger_email_notification(data);
            } else {
                Serial.println("[EMAIL] Warning alert diabaikan (rate limit).");
            }
            lastNotifState = NOTIF_WARNING;
        }
    } else {
        if (lastNotifState != NOTIF_NORMAL) {
            send_notification("info", "Humidity back to normal.", humidity, currentTemperature);
            NotificationData data;
            data.type = "info";
            data.message = "Kondisi kelembapan kembali normal.";
            data.humidity = humidity;
            data.temperature = currentTemperature;
            if (can_send_email(lastEmailSent_normal, EMAIL_MIN_INTERVAL_ALERT_MS)) {
                trigger_email_notification(data);
            } else {
                Serial.println("[EMAIL] Normal info diabaikan (rate limit).");
            }
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
                    char msg[SCHEDULE_MSG_SIZE];
                    snprintf(msg, SCHEDULE_MSG_SIZE, "Scheduled watering at %d:00 skipped, humidity critical (%.1f%%).", currentHour, humidity);
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
    pumpCountdownSeconds = PUMP_DURATION_MS / 1000;
    pumpCountdownLastPublish = 0;
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    mqttClient.publish(TOPICS.status, "{\"state\":\"pumping\"}");
    char msg[PUMP_MSG_SIZE];
    snprintf(msg, PUMP_MSG_SIZE, "Pump turned ON (%s).", reason);
    send_notification("info", msg, currentHumidity, currentTemperature);
    publish_pump_countdown(pumpCountdownSeconds);
}
void turn_pump_off() {
    if (!isPumpOn) return;
    isPumpOn = false;
    digitalWrite(PUMP_RELAY_PIN, LOW);
    mqttClient.publish(TOPICS.status, "{\"state\":\"idle\"}");
    send_notification("info", "Pump turned OFF.", currentHumidity, currentTemperature);
    publish_pump_countdown(0);
}

// =================================================================
//   FUNGSI KOMUNIKASI, MQTT, NOTIFIKASI, UTILITAS
// =================================================================
void try_reconnect_mqtt() {
    if (!mqttClient.connected() && millis() - lastMqttRetryTime > MQTT_RETRY_INTERVAL) {
        lastMqttRetryTime = millis();
        Serial.print("Mencoba koneksi MQTT (TLS)...");
        if (mqttClient.connect(
                mqttClientId,
                MQTT_USER,
                MQTT_PASSWORD,
                TOPICS.status, // LWT topic
                1,             // QoS
                true,          // retained
                "{\"state\":\"offline\"}" // LWT payload
            )) {
            Serial.println("terhubung!");
            mqttClient.publish(TOPICS.status, "{\"state\":\"online\"}", true); // Retained!
            mqttClient.subscribe(TOPICS.pump_control);
            mqttClient.subscribe(TOPICS.config_set);
            mqttClient.subscribe(TOPICS.system_update);
            mqttClient.subscribe(TOPICS.firmware_new);
            publish_config();
            publish_current_version();
            Serial.println("Berlangganan topik MQTT sukses.");
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
        JsonDocument doc;
        deserializeJson(doc, payload, length);
        if (!doc["command"].isNull() && doc["command"] == "FIRMWARE_UPDATE") {
            String url = doc["url"];
            if (url.length() > 0) {
                perform_ota_update(url);
            }
        }
    } else if (strcmp(topic, TOPICS.firmware_new) == 0) {
        JsonDocument doc;
        deserializeJson(doc, payload, length);
        if (!doc["version"].isNull()) {
            newFirmware.version = doc["version"].as<String>();
            newFirmware.release_notes = doc["release_notes"] | "";
            newFirmware.url = doc["url"] | "";
            check_for_firmware_update();
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


// =================================================================
//   FUNGSI KOMUNIKASI, MQTT, NOTIFIKASI, UTILITAS
// =================================================================
void publish_telemetry() {
    char payload[TELEMETRY_PAYLOAD_SIZE];
    snprintf(payload, TELEMETRY_PAYLOAD_SIZE, "{\"temperature\":%.2f, \"humidity\":%.2f}", currentTemperature, currentHumidity);
    mqttClient.publish(TOPICS.telemetry, payload);
}

void publish_wifi_signal() {
    long rssi = WiFi.RSSI();
    char payload[WIFI_SIGNAL_PAYLOAD_SIZE];
    snprintf(payload, WIFI_SIGNAL_PAYLOAD_SIZE, "{\"rssi\":%ld}", rssi);
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
    char buffer[CONFIG_BUFFER_SIZE];
    serializeJson(doc, buffer);
    mqttClient.publish(TOPICS.config_get, buffer, true);
}

void publish_current_version() {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    char payload[VERSION_PAYLOAD_SIZE];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_current, payload, true); 
    Serial.printf("Versi firmware saat ini (%s) dipublikasikan.\n", FIRMWARE_VERSION);
}

void publish_firmware_status(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    doc["version"] = FIRMWARE_VERSION;
    char payload[FIRMWARE_STATUS_PAYLOAD_SIZE];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_current, payload, true);
}

void publish_firmware_update_progress(const char* stage, int progress, const char* message) {
    JsonDocument doc;
    doc["stage"] = stage;
    doc["progress"] = progress;
    if (message && strlen(message) > 0) doc["message"] = message;
    char payload[FIRMWARE_UPDATE_PAYLOAD_SIZE];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_update, payload, true);
}

void trigger_email_notification(const NotificationData& data) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("Tidak bisa kirim email, WiFi tidak terhubung.");
        return;
    }
    HTTPClient http;
    String functionUrl = String(SUPABASE_URL) + "/functions/v1/send-email-notification";
    Serial.printf("Memicu notifikasi email tipe: %s\n", data.type);
    http.begin(functionUrl);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    JsonDocument doc;
    doc["type"] = data.type;
    doc["message"] = data.message;
    if (data.humidity >= 0) doc["humidity"] = data.humidity;
    if (data.temperature >= 0) doc["temperature"] = data.temperature;
    if (data.version.length() > 0) doc["version"] = data.version;
    if (data.release_notes.length() > 0) doc["release_notes"] = data.release_notes;
    String jsonPayload;
    serializeJson(doc, jsonPayload);
    int httpCode = http.POST(jsonPayload);
    if (httpCode >= 200 && httpCode < 300) {
        Serial.printf("[HTTP] Notifikasi email berhasil dikirim, Kode: %d\n", httpCode);
    } else {
        Serial.printf("[HTTP] Pengiriman gagal, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

void check_for_firmware_update() {
    if (newFirmware.version.length() > 0 && newFirmware.version != FIRMWARE_VERSION) {
        Serial.printf("Firmware baru terdeteksi! Saat ini: %s, Tersedia: %s\n", FIRMWARE_VERSION, newFirmware.version.c_str());
        NotificationData data;
        data.type = "firmware_update";
        data.message = "Firmware update tersedia!";
        data.version = newFirmware.version;
        data.release_notes = newFirmware.release_notes;
        if (can_send_email(lastEmailSent_firmware, EMAIL_MIN_INTERVAL_FIRMWARE_MS)) {
            trigger_email_notification(data);
        } else {
            Serial.println("[EMAIL] Firmware update diabaikan (rate limit).");
        }
        newFirmware.version = ""; 
    }
}

void lcd_show_message(const char* line1, const char* line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}

void pause_and_restart(unsigned long ms) {
    delay(ms);
    ESP.restart();
}

void publish_online_status() {
    mqttClient.publish(TOPICS.status, "{\"state\":\"online\"}", true);
}

void publish_speedtest(float ping_ms, float download_mbps, float upload_mbps) {
    char payload[196];
    snprintf(payload, sizeof(payload),
        "{\"ping_ms\":%.2f,\"download_mbps\":%.2f,\"upload_mbps\":%.2f,\"lat\":%.6f,\"lon\":%.6f}",
        ping_ms, download_mbps, upload_mbps, DEVICE_LATITUDE, DEVICE_LONGITUDE);
    mqttClient.publish(TOPICS.speedtest, payload, true);
}



// =================================================================
//   FUNGSI OTA UPDATE
// =================================================================
void handle_ota_error(const char* lcdMsg, const char* logMsg, int code = 0) {
    lcd_show_message("Update Failed!", "Check Serial Mon.");
    if (code)
        Serial.printf(logMsg, code);
    else
        Serial.println(logMsg);
    publish_firmware_status("failed");
    publish_firmware_update_progress("error", 0, lcdMsg);
    send_notification("error", lcdMsg);
    pause_and_restart(ERROR_RESTART_DELAY);
}

void perform_ota_update(String url) {
    int retry = 0;
    bool success = false;

    while (retry < OTA_MAX_RETRY && !success) {
        HTTPClient http;
        http.setTimeout(OTA_HTTP_TIMEOUT_MS); // Timeout dari config.h
        http.begin(url);
        int httpCode = http.GET();

        if (httpCode != HTTP_CODE_OK) {
            Serial.printf("OTA HTTP GET gagal (percobaan %d), code: %d\n", retry+1, httpCode);
            http.end();
            retry++;
            delay(2000);
            continue;
        }

        int contentLength = http.getSize();
        if (contentLength <= 0) {
            Serial.println("Content length tidak diketahui.");
            http.end();
            retry++;
            delay(2000);
            continue;
        }

        if ((uint32_t)ESP.getFreeSketchSpace() < (uint32_t)contentLength) {
            Serial.println("Tidak cukup ruang untuk update.");
            http.end();
            break;
        }

        if (!Update.begin(contentLength)) {
            Serial.println("Memori tidak cukup untuk update.");
            http.end();
            retry++;
            delay(2000);
            continue;
        }

        WiFiClient& stream = http.getStream();
        size_t written = 0;
        uint8_t buff[OTA_BUFFER_SIZE];
        int lastPercent = 0;
        unsigned long lastProgressTime = millis();
        bool streamError = false;

        while (written < (size_t)contentLength) {
            size_t toRead = OTA_BUFFER_SIZE;
            if ((contentLength - written) < toRead) toRead = contentLength - written;
            int bytesRead = stream.readBytes(buff, toRead);
            if (bytesRead <= 0) {
                Serial.println("Gagal membaca stream data, coba ulang...");
                Update.abort();
                http.end();
                retry++;
                delay(2000);
                streamError = true;
                break;
            }
            if (Update.write(buff, bytesRead) != bytesRead) {
                Serial.println("Gagal menulis data ke flash, coba ulang...");
                Update.abort();
                http.end();
                retry++;
                delay(2000);
                streamError = true;
                break;
            }
            written += bytesRead;
            int percent = (int)(100.0 * written / contentLength);
            if (percent != lastPercent && millis() - lastProgressTime > 200) {
                publish_firmware_update_progress("downloading", percent, "Downloading...");
                lastPercent = percent;
                lastProgressTime = millis();
            }
        }

        if (!streamError && written == (size_t)contentLength && Update.end() && Update.isFinished()) {
            Serial.println("Update successful! Restarting...");
            lcd_show_message("Update Success!", "Restarting...");
            publish_firmware_status("updated");
            publish_firmware_update_progress("finished", 100, "Update selesai, restart...");
            send_notification("info", "Firmware updated successfully.");
            http.end();
            delay(2000);
            ESP.restart();
            success = true;
        } else if (!streamError) {
            Serial.println("Update gagal menyelesaikan proses, coba ulang...");
            Update.abort();
            http.end();
            retry++;
            delay(2000);
        }
    }
    if (!success) {
        lcd_show_message("OTA Gagal!", "Cek WiFi/Server");
        Serial.println("OTA gagal setelah beberapa percobaan.");
        publish_firmware_status("failed");
        publish_firmware_update_progress("error", 0, "OTA gagal setelah beberapa percobaan.");
        send_notification("error", "OTA gagal setelah beberapa percobaan.");
        delay(10000); // Delay lebih lama sebelum restart
        ESP.restart();
    }
}


// =============================
// == FUNGSI SPEEDTEST OTOMATIS ==
// =============================
float speedtest_ping_ms(const char* host = "8.8.8.8", uint16_t port = 53, uint8_t count = 4) {
    WiFiClient client;
    unsigned long total = 0;
    int success = 0;
    for (uint8_t i = 0; i < count; i++) {
        unsigned long start = millis();
        if (client.connect(host, port)) {
            unsigned long elapsed = millis() - start;
            total += elapsed;
            success++;
            client.stop();
        }
        delay(100);
    }
    return (success > 0) ? (float)total / success : -1.0f;
}

float speedtest_download_mbps(const char* url = SPEEDTEST_DOWNLOAD_URL, size_t test_size = SPEEDTEST_DOWNLOAD_SIZE) {
    HTTPClient http;
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    http.begin(url);
    unsigned long start = millis();
    int httpCode = http.GET();
    float mbps = -1.0f;
    if (httpCode == HTTP_CODE_OK) {
        WiFiClient* stream = http.getStreamPtr();
        size_t total = 0;
        uint8_t buf[256];
        while (total < test_size) {
            int len = stream->read(buf, sizeof(buf));
            if (len <= 0) break;
            total += len;
        }
        unsigned long elapsed = millis() - start;
        if (elapsed > 0 && total > 0) {
            mbps = (total * 8.0f / 1000000.0f) / (elapsed / 1000.0f); // Mbps
        }
    }
    http.end();
    return mbps;
}

float speedtest_upload_mbps(const char* url = SPEEDTEST_UPLOAD_URL, size_t test_size = SPEEDTEST_UPLOAD_SIZE) {
    HTTPClient http;
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);
    http.begin(url);
    char* payload = new char[test_size + 1];
    memset(payload, 'A', test_size);
    payload[test_size] = '\0';
    unsigned long start = millis();
    int httpCode = http.POST((uint8_t*)payload, test_size);
    unsigned long elapsed = millis() - start;
    float mbps = -1.0f;
    if (httpCode > 0 && elapsed > 0) {
        mbps = (test_size * 8.0f / 1000000.0f) / (elapsed / 1000.0f); // Mbps
    }
    http.end();
    delete[] payload;
    return mbps;
}

void run_and_publish_speedtest() {
    float ping = speedtest_ping_ms();
    float download = speedtest_download_mbps();
    float upload = speedtest_upload_mbps();
    publish_speedtest(ping, download, upload);
    Serial.printf("Speedtest: ping=%.2f ms, download=%.2f Mbps, upload=%.2f Mbps\n", ping, download, upload);
}
