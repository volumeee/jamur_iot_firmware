#include "functions.h"
#include "notif.h"
#include "config.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>

bool publish_with_retry(const char* topic, const char* payload, bool retained, int retry, int delayMs) {
    for (int i = 0; i < retry; ++i) {
        if (mqttClient.publish(topic, payload, retained)) {
            return true;
        }
        delay(delayMs);
    }
    return false;
}

void send_notification(const char* type, const char* message, float humidity, float temperature) {
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
        trigger_email_notification(data);
        newFirmware.version = "";
    }
}

void publish_current_version() {
    JsonDocument doc;
    doc["version"] = FIRMWARE_VERSION;
    char payload[50];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_current, payload, true);
} 