#include "functions.h"
#include "button.h"
#include "config.h"
#include <Arduino.h>

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
                display_normal_info();
            }
        }
        lastDebounceTime = millis();
    }
} 