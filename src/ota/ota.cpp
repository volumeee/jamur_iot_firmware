#include "functions.h"
#include "ota.h"
#include "config.h"
#include <HTTPClient.h>
#include <Update.h>
#include <WiFiClient.h>
#include <LiquidCrystal_I2C.h>
#include <ArduinoJson.h>

void publish_ota_progress(const char* stage, int percent, const char* message) {
    JsonDocument doc;
    doc["stage"] = stage;
    doc["progress"] = percent;
    doc["message"] = message;
    char payload[128];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_update, payload, true);
}

void publish_firmware_status(const char* status) {
    JsonDocument doc;
    doc["status"] = status;
    char payload[64];
    serializeJson(doc, payload);
    mqttClient.publish(TOPICS.firmware_update, payload, true);
}

void publish_ota_update(String url) {
    Serial.printf("Received OTA Update command. URL: %s\n", url.c_str());
    currentState = STATE_UPDATING;
    publish_firmware_status("updating");
    publish_ota_progress("downloading", 0, "Mulai download firmware");
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
                publish_ota_progress("error", 0, "No space for OTA");
                http.end();
                delay(5000);
                currentState = STATE_NORMAL_OPERATION;
                return;
            }
            bool canBegin = Update.begin(contentLength);
            if (canBegin) {
                WiFiClient& stream = http.getStream();
                size_t written = 0;
                int lastPercent = 0;
                uint8_t buf[1024];
                int totalRead = 0;
                while (totalRead < contentLength) {
                    int toRead = min((int)sizeof(buf), contentLength - totalRead);
                    int bytesRead = stream.read(buf, toRead);
                    if (bytesRead <= 0) break;
                    if (Update.write(buf, bytesRead) != (size_t)bytesRead) {
                        Serial.println("Update write error!");
                        publish_ota_progress("error", lastPercent, "Update write error");
                        break;
                    }
                    totalRead += bytesRead;
                    int percent = (totalRead * 100) / contentLength;
                    if (percent - lastPercent >= 10 || percent == 100) {
                        lastPercent = percent;
                        publish_ota_progress("downloading", percent, "Downloading firmware");
                    }
                }
                if (totalRead == contentLength) {
                    Serial.println("Firmware download successful.");
                    publish_ota_progress("downloading", 100, "Download selesai");
                } else {
                    Serial.println("Firmware download failed, size mismatch.");
                    publish_ota_progress("error", lastPercent, "Download size mismatch");
                }
                publish_ota_progress("installing", 0, "Mulai install firmware");
                if (Update.end()) {
                    if (Update.isFinished()) {
                        Serial.println("Update successful! Restarting...");
                        lcd.clear();
                        lcd.print("Update Success!");
                        lcd.setCursor(0, 1);
                        lcd.print("Restarting...");
                        publish_ota_progress("finished", 100, "Update sukses, restart");
                        publish_firmware_status("updated");
                        send_notification("info", "Firmware updated successfully.");
                        delay(2000);
                        ESP.restart();
                    } else {
                        Serial.println("Failed to finish update.");
                        publish_ota_progress("error", 100, "Update not finished");
                    }
                } else {
                    Serial.printf("OTA Error: #%u\n", Update.getError());
                    publish_ota_progress("error", lastPercent, "Update.end() error");
                }
            } else {
                Serial.println("Not enough memory for OTA.");
                publish_ota_progress("error", 0, "Not enough memory");
            }
        } else {
            Serial.println("Unknown content length.");
            publish_ota_progress("error", 0, "Unknown content length");
        }
    } else {
        Serial.printf("HTTP GET failed, error code: %d\n", httpCode);
        publish_ota_progress("error", 0, "HTTP GET failed");
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