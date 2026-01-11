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

static const char *TAG = "PRIORITY_GUARDIAN";

/* ================= WIFI ================= */
#define WIFI_CONNECTED_BIT BIT0
static EventGroupHandle_t wifi_event_group;

/* ================= MQTT ================= */
static esp_mqtt_client_handle_t mqtt_client;

/* ================= QUEUES ================= */
static QueueHandle_t mqtt_dispatch_queue;
static QueueHandle_t stream_queue;
static QueueHandle_t distress_queue;

/* ================= STRUCTS ================= */
typedef enum
{
    MQTT_MSG_STREAM,
    MQTT_MSG_DISTRESS,
    MQTT_MSG_OTHER
} mqtt_msg_type_t;

typedef struct
{
    mqtt_msg_type_t type;
    char data[64];
} mqtt_dispatch_msg_t;

typedef struct
{
    int64_t rx_time_ms;
} distress_msg_t;

/* ================= WIFI HANDLER ================= */
static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
        esp_wifi_connect();

    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
        esp_wifi_connect();

    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
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

    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
}

/* ================= MQTT EVENT (MINIMAL) ================= */
static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    if (event->event_id == MQTT_EVENT_CONNECTED)
    {
        ESP_LOGI(TAG, "MQTT connected");
        esp_mqtt_client_subscribe(mqtt_client, STREAM_TOPIC, 1);
        esp_mqtt_client_subscribe(mqtt_client, DISTRESS_TOPIC, 1);
        return;
    }

    if (event->event_id == MQTT_EVENT_DISCONNECTED)
    {
        ESP_LOGE(TAG, "MQTT DISCONNECTED");
    }

    if (event->event_id != MQTT_EVENT_DATA)
        return;

    mqtt_dispatch_msg_t msg = {0};

    if (event->topic_len == strlen(STREAM_TOPIC) &&
        strncmp(event->topic, STREAM_TOPIC, event->topic_len) == 0)
    {
        msg.type = MQTT_MSG_STREAM;
    }
    else if (event->topic_len == strlen(DISTRESS_TOPIC) &&
             strncmp(event->topic, DISTRESS_TOPIC, event->topic_len) == 0 &&
             strstr(event->data, "CHALLENGE"))
    {
        msg.type = MQTT_MSG_DISTRESS;
    }
    else
    {
        msg.type = MQTT_MSG_OTHER;
    }

    strncpy(msg.data, event->data, sizeof(msg.data) - 1);
    xQueueSend(mqtt_dispatch_queue, &msg, 0);
}

/* ================= MQTT INIT ================= */
static void mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        .session.keepalive = 15,
        .network.disable_auto_reconnect = false,
    };

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(
        mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    esp_mqtt_client_start(mqtt_client);
}

/* ================= PRIORITY 2: MQTT DISPATCHER ================= */
static void mqtt_dispatch_task(void *arg)
{
    mqtt_dispatch_msg_t msg;

    while (1)
    {
        if (xQueueReceive(mqtt_dispatch_queue, &msg, portMAX_DELAY))
        {
            if (msg.type == MQTT_MSG_STREAM)
            {
                float value = atof(msg.data);
                xQueueSend(stream_queue, &value, 0);
            }
            else if (msg.type == MQTT_MSG_DISTRESS)
            {
                distress_msg_t d;
                d.rx_time_ms = esp_timer_get_time() / 1000;
                xQueueSend(distress_queue, &d, portMAX_DELAY);
            }
            else
            {
                ESP_LOGW(TAG, "NEXT CHALLENGE / OTHER MSG: %s", msg.data);
            }
        }
    }
}

/* ================= PRIORITY 1: STREAM TASK ================= */
static void stream_task(void *arg)
{
    float window[ROLLING_WINDOW] = {0};
    int count = 0, index = 0, msg_num = 0;
    float value;

    while (1)
    {
        if (xQueueReceive(stream_queue, &value, portMAX_DELAY))
        {
            msg_num++;

            window[index] = value;
            index = (index + 1) % ROLLING_WINDOW;
            if (count < ROLLING_WINDOW)
                count++;

            float sum = 0;
            for (int i = 0; i < count; i++)
                sum += window[i];

            float avg = sum / count;

            printf("Message %d: %.2f  -> Average: %.2f\n",
                   msg_num, value, avg);
        }
    }
}

/* ================= PRIORITY 3: DISTRESS TASK ================= */
static void distress_task(void *arg)
{
    distress_msg_t msg;

    while (1)
    {
        if (xQueueReceive(distress_queue, &msg, portMAX_DELAY))
        {
            LED_ON(LED_GPIO);

            int64_t ack_time_ms = esp_timer_get_time() / 1000;

            char payload[128];
            snprintf(payload, sizeof(payload),
                     "{\"status\":\"ACK\",\"timestamp_ms\":%lld}",
                     ack_time_ms);

            esp_mqtt_client_publish(
                mqtt_client,
                ACK_TOPIC,
                payload,
                0,
                1,
                0);

            ESP_LOGI(TAG,
                     "DISTRESS RX=%lld ms | ACK SENT=%lld ms",
                     msg.rx_time_ms,
                     ack_time_ms);

            LED_OFF(LED_GPIO);
        }
    }
}

/* ================= MAIN ================= */
void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    LED_OFF(LED_GPIO);

    mqtt_dispatch_queue = xQueueCreate(10, sizeof(mqtt_dispatch_msg_t));
    stream_queue = xQueueCreate(10, sizeof(float));
    distress_queue = xQueueCreate(5, sizeof(distress_msg_t));

    wifi_init();

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        false,
        true,
        portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected, starting MQTT");
    mqtt_init();

    xTaskCreate(mqtt_dispatch_task, "mqtt_dispatch",
                4096, NULL, PRIORITY_MQTT, NULL);

    xTaskCreate(stream_task, "stream_task",
                4096, NULL, PRIORITY_STREAM, NULL);

    xTaskCreate(distress_task, "distress_task",
                4096, NULL, PRIORITY_DISTRESS, NULL);
}
