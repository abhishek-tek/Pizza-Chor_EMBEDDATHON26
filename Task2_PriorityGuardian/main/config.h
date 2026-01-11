#pragma once

// Config ------------------------------------------------------------------- //

#define WIFI_SSID "Redmi Note 12 5G"
#define WIFI_PASS "12345678"

#define MQTT_BROKER_URI "mqtt://broker.mqttdashboard.com:1883"

#define STREAM_TOPIC "krillparadise/data/stream"
#define DISTRESS_TOPIC "shouryadippizzachor"
#define ACK_TOPIC "shouryadipchakrabortypizzachor"

#define LED_GPIO 21

#define ROLLING_WINDOW 10

#define PRIORITY_STREAM 1
#define PRIORITY_MQTT 2
#define PRIORITY_DISTRESS 3

#define LED_ACTIVE_HIGH 0 // Common Anode

#if LED_ACTIVE_HIGH
#define LED_ON(pin) gpio_set_level(pin, 1)
#define LED_OFF(pin) gpio_set_level(pin, 0)
#else
#define LED_ON(pin) gpio_set_level(pin, 0)
#define LED_OFF(pin) gpio_set_level(pin, 1)
#endif
