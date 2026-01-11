#pragma once

// Config ------------------------------------------------------------------- //

#define WIFI_SSID "Redmi Note 12 5G"
#define WIFI_PASS "12345678"

#define MQTT_BROKER_URI "mqtt://broker.mqttdashboard.com:1883"

#define WINDOW_TOPIC "mnjki_window"
#define SYNC_PUB_TOPIC "cagedmonkey/listener"

#define BUTTON_GPIO 15

#define LED_RED 21
#define LED_GREEN 19
#define LED_BLUE 18

#define WINDOW_TOLERANCE_MS 50
#define WINDOW_MAX_MS 1100
#define DEBOUNCE_MS 20

#define PRIORITY_MQTT 2
#define PRIORITY_BUTTON 3
#define PRIORITY_WINDOW 4

#define LED_ACTIVE_HIGH 0 // Common Anode

#if LED_ACTIVE_HIGH
#define LED_ON(pin) gpio_set_level(pin, 1)
#define LED_OFF(pin) gpio_set_level(pin, 0)
#else
#define LED_ON(pin) gpio_set_level(pin, 0)
#define LED_OFF(pin) gpio_set_level(pin, 1)
#endif
