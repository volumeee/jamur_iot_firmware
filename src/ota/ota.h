#ifndef OTA_H
#define OTA_H
#include <Arduino.h>
#include "functions.h"

void publish_ota_progress(const char* stage, int percent, const char* message);
void publish_firmware_status(const char* status);
void publish_ota_update(String url);

#endif // OTA_H 