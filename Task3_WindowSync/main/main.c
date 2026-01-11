#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "driver/gpio.h"
#include "config.h"

static const char *TAG = "WINDOW_SYNC";
static int64_t window_close_time = 0;

/* ================= WIFI ================= */
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

/* ================= MQTT ================= */
static esp_mqtt_client_handle_t mqtt_client;

/* ================= QUEUES ================= */
static QueueHandle_t window_queue;
static QueueHandle_t button_queue;

/* ================= STATE ================= */
static volatile bool window_open = false;
static int64_t window_open_time = 0;

/* ================= STRUCTS ================= */
typedef struct
{
    int64_t timestamp_ms;
} button_event_t;

/* ================= WIFI ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t base,
                               int32_t id,
                               void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();

    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP)
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
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
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); // IMPORTANT
}

/* ================= MQTT ================= */
static void mqtt_event_handler(void *arg,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *data)
{
    esp_mqtt_event_handle_t event = data;

    if (event_id != MQTT_EVENT_DATA)
        return;

    char topic[event->topic_len + 1];
    memcpy(topic, event->topic, event->topic_len);
    topic[event->topic_len] = '\0';

    char payload[event->data_len + 1];
    memcpy(payload, event->data, event->data_len);
    payload[event->data_len] = '\0';

    ESP_LOGI(TAG, "MQTT RX | topic='%s' payload='%s'", topic, payload);

    if (strcmp(topic, WINDOW_TOPIC) == 0)
    {
        if (strstr(payload, "open"))
        {
            int64_t now = esp_timer_get_time() / 1000;
            xQueueSend(window_queue, &now, 0);
        }
    }
}

static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.keepalive = 15,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(mqtt_client);
    esp_mqtt_client_subscribe(mqtt_client, WINDOW_TOPIC, 1);
}

/* ================= BUTTON ISR ================= */
static void IRAM_ATTR button_isr(void *arg)
{
    static int64_t last_press = 0;
    int64_t now = esp_timer_get_time() / 1000;

    if (now - last_press < DEBOUNCE_MS)
        return;

    last_press = now;

    button_event_t evt = {.timestamp_ms = now};
    xQueueSendFromISR(button_queue, &evt, NULL);
}

/* ================= WINDOW TASK ================= */
static void window_task(void *arg)
{
    int64_t open_time;

    while (1)
    {
        if (xQueueReceive(window_queue, &open_time, portMAX_DELAY))
        {
            window_open = true;
            window_open_time = open_time;

            LED_ON(LED_BLUE);
            LED_OFF(LED_RED);

            ESP_LOGI(TAG, "WINDOW OPEN @ %lld ms", open_time);

            vTaskDelay(pdMS_TO_TICKS(1100)); // max window

            window_close_time = esp_timer_get_time() / 1000;
            window_open = false;

            LED_OFF(LED_BLUE);
            LED_ON(LED_RED);

            ESP_LOGI(TAG, "WINDOW CLOSED");
        }
    }
}

/* ================= BUTTON TASK ================= */
static void button_task(void *arg)
{
    button_event_t evt;

    while (1)
    {
        if (xQueueReceive(button_queue, &evt, portMAX_DELAY))
        {
            LED_ON(LED_GREEN);
            vTaskDelay(pdMS_TO_TICKS(50));
            LED_OFF(LED_GREEN);

            if (window_open)
            {
                int64_t delta = llabs(evt.timestamp_ms - window_open_time);

                if (delta <= WINDOW_TOLERANCE_MS)
                {
                    char payload[128];
                    snprintf(payload, sizeof(payload),
                             "{\"status\":\"synced\",\"timestamp_ms\":%lld}",
                             evt.timestamp_ms);

                    esp_mqtt_client_publish(
                        mqtt_client,
                        SYNC_PUB_TOPIC,
                        payload,
                        0,
                        1,
                        0);

                    ESP_LOGI(TAG,
                             "SYNC SUCCESS | delta=%lld ms",
                             delta);
                }
                else
                {
                    ESP_LOGW(TAG,
                             "MISS | delta=%lld ms",
                             delta);
                }
            }
        }
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_set_direction(LED_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE, GPIO_MODE_OUTPUT);

    LED_ON(LED_RED);
    LED_OFF(LED_GREEN);
    LED_OFF(LED_BLUE);

    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    gpio_config(&btn_cfg);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, button_isr, NULL);

    window_queue = xQueueCreate(5, sizeof(int64_t));
    button_queue = xQueueCreate(5, sizeof(button_event_t));

    wifi_init();

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        false,
        true,
        portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected, starting MQTT");
    mqtt_init();

    xTaskCreate(window_task, "window_task", 4096, NULL,
                PRIORITY_WINDOW, NULL);

    xTaskCreate(button_task, "button_task", 4096, NULL,
                PRIORITY_BUTTON, NULL);
}
