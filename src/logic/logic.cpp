#include "functions.h"
#include "logic.h"
#include "config.h"
#include "notif/notif.h"
#include <Arduino.h>

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
            trigger_email_notification(data);
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
            trigger_email_notification(data);
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
            trigger_email_notification(data);
            lastNotifState = NOTIF_NORMAL;
        }
    }
}

void run_scheduled_control(float humidity) {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    int currentHour = timeinfo.tm_hour;
    static int lastScheduledHour = -1;
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