#pragma once

#ifdef RGB_MATRIX_ENABLE

// RGB LED locations for Adafruit Macropad RP2040
enum led_location_map {
    LED_K00, LED_K01, LED_K02,
    LED_K10, LED_K11, LED_K12,
    LED_K20, LED_K21, LED_K22,
    LED_K30, LED_K31, LED_K32
};

// Define specific LEDs used in a particular layer
// (static: this header is included by more than one .c file)
static const uint8_t LED_LAYER[] = {
    LED_K00,
    LED_K01,
    LED_K02,
    LED_K10,
    LED_K11,
    LED_K12     // Pomodoro layer
};
#endif
