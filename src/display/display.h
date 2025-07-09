#ifndef DISPLAY_H
#define DISPLAY_H
#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include "functions.h"

void display_boot_screen();
void display_connecting_wifi();
void display_ap_info(IPAddress ip);
void display_normal_info();
void display_menu_info();

#endif // DISPLAY_H 