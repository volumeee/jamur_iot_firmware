#ifndef NOTIF_H
#define NOTIF_H
#include <Arduino.h>
#include <PubSubClient.h>
#include "config.h"
#include "functions.h"

bool publish_with_retry(const char* topic, const char* payload, bool retained = false, int retry = NOTIF_RETRY_COUNT, int delayMs = NOTIF_RETRY_DELAY_MS);
void send_notification(const char* type, const char* message, float humidity, float temperature);
void trigger_email_notification(const NotificationData& data);
void check_for_firmware_update();

#endif // NOTIF_H 