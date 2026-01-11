#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_log.h"

#include "driver/gpio.h"
#include "cJSON.h"

#include "config.h"

static const char *TAG = "TIMING_KEEPER";

// WiFi sync ---------------------------------------------------------------- //
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

// LED pattern -------------------------------------------------------------- //
typedef struct
{
    uint32_t durations[MAX_PATTERN_LEN];
    uint8_t length;
} led_pattern_t;

static led_pattern_t red_pattern = {.length = 0};
static led_pattern_t green_pattern = {.length = 0};
static led_pattern_t blue_pattern = {.length = 0};

static SemaphoreHandle_t pattern_mutex;

// MQTT --------------------------------------------------------------------- //
static esp_mqtt_client_handle_t mqtt_client;

// WIFI --------------------------------------------------------------------- //
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi disconnected, retrying...");
        esp_wifi_connect();
    }

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ESP_LOGI(TAG, "WiFi got IP");
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// MQTT --------------------------------------------------------------------- //
static void parse_pattern(cJSON *array, led_pattern_t *pattern)
{
    int count = cJSON_GetArraySize(array);
    if (count > MAX_PATTERN_LEN)
    {
        count = MAX_PATTERN_LEN;
    }

    pattern->length = count;
    for (int i = 0; i < count; i++)
    {
        pattern->durations[i] = cJSON_GetArrayItem(array, i)->valueint;
    }
}

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    if (event->event_id == MQTT_EVENT_DATA)
    {
        char *payload = strndup(event->data, event->data_len);
        ESP_LOGI(TAG, "MQTT DATA: %s", payload);

        cJSON *root = cJSON_Parse(payload);
        free(payload);

        if (!root)
        {
            return;
        }

        xSemaphoreTake(pattern_mutex, portMAX_DELAY);

        cJSON *r = cJSON_GetObjectItem(root, "red");
        cJSON *g = cJSON_GetObjectItem(root, "green");
        cJSON *b = cJSON_GetObjectItem(root, "blue");

        if (r)
        {
            parse_pattern(r, &red_pattern);
        }
        if (g)
        {
            parse_pattern(g, &green_pattern);
        }
        if (b)
        {
            parse_pattern(g, &blue_pattern);
        }

        xSemaphoreGive(pattern_mutex);
        cJSON_Delete(root);
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = "mqtt://broker.mqttdashboard.com:1883",
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
    esp_mqtt_client_subscribe(mqtt_client, MQTT_TOPIC, 0);
}

// LED Task ----------------------------------------------------------------- //
static void led_task(void *arg)
{
    gpio_num_t pin = (gpio_num_t)arg;
    led_pattern_t *pattern;

    if (pin == RED_PIN)
    {
        pattern = &red_pattern;
    }
    else if (pin == GREEN_PIN)
    {
        pattern = &green_pattern;
    }
    else
    {
        pattern = &blue_pattern;
    }

    uint8_t idx = 0;
    uint8_t last_length = 0;

    while (1)
    {
        xSemaphoreTake(pattern_mutex, portMAX_DELAY);

        if (pattern->length == 0)
        {
            xSemaphoreGive(pattern_mutex);
            LED_OFF(pin);
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        if (pattern->length != last_length)
        {
            idx = 0;
            last_length = pattern->length;
        }

        uint32_t duration = pattern->durations[idx];
        xSemaphoreGive(pattern_mutex);

        LED_ON(pin);
        vTaskDelay(pdMS_TO_TICKS(duration));

        LED_OFF(pin);
        vTaskDelay(pdMS_TO_TICKS(duration));

        idx = (idx + 1) % pattern->length;
    }
}

// Main --------------------------------------------------------------------- //
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    pattern_mutex = xSemaphoreCreateMutex();

    gpio_config_t io_conf = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL << RED_PIN) |
            (1ULL << GREEN_PIN) |
            (1ULL << BLUE_PIN),
    };
    gpio_config(&io_conf);

    wifi_init();

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        false,
        true,
        portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected, starting MQTT");
    mqtt_init();

    xTaskCreate(led_task, "red_led", 2048, (void *)RED_PIN, 5, NULL);
    xTaskCreate(led_task, "green_led", 2048, (void *)GREEN_PIN, 5, NULL);
    xTaskCreate(led_task, "blue_led", 2048, (void *)BLUE_PIN, 5, NULL);
}
