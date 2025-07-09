#ifndef LOGIC_H
#define LOGIC_H
#include <Arduino.h>
#include "config.h"
#include "functions.h"

void run_humidity_control_logic(float humidity);
void run_scheduled_control(float humidity);
void turn_pump_on(const char* reason);
void turn_pump_off();

#endif // LOGIC_H 