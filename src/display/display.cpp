#include "functions.h"
#include "display.h"
#include "config.h"
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>

void display_boot_screen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Jamur IoT ");
    lcd.print(FIRMWARE_VERSION);
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