#pragma once

// Config ------------------------------------------------------------------- //

#define WIFI_SSID "A"
#define WIFI_PASS "12345678"

#define MQTT_TOPIC "shrimphub/led/timing/set"

#define RED_PIN 21
#define GREEN_PIN 19
#define BLUE_PIN 18

#define MAX_PATTERN_LEN 16

#define LED_ACTIVE_HIGH 0 // Common Anode

#if LED_ACTIVE_HIGH
#define LED_ON(pin) gpio_set_level(pin, 1)
#define LED_OFF(pin) gpio_set_level(pin, 0)
#else
#define LED_ON(pin) gpio_set_level(pin, 0)
#define LED_OFF(pin) gpio_set_level(pin, 1)
#endif
