#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "mqtt_client.h"
#include "cJSON.h"
#include "mbedtls/base64.h"

#include "config.h"

static const char *TAG = "TASK4";

// --------- Image Buffers ---------
static char *image_b64 = NULL;
static size_t image_b64_len = 0;
static uint8_t *image_bin = NULL;
static size_t image_bin_len = 0;

// --------- MQTT Client ---------
static esp_mqtt_client_handle_t mqtt_client;

// ------------------------------------------------------------

static void publish_task4_request(void)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "request", TASK3_HIDDEN_MESSAGE);
    cJSON_AddStringToObject(root, "agent_id", TEAM_ID);

    char *json = cJSON_PrintUnformatted(root);

    esp_mqtt_client_publish(
        mqtt_client,
        TASK4_REQUEST_TOPIC,
        json,
        0,
        1,
        0);

    ESP_LOGI(TAG, "Published Task 4 request");
    ESP_LOGI(TAG, "Payload: %s", json);

    cJSON_Delete(root);
    free(json);
}

// ------------------------------------------------------------

static void try_decode_image(void)
{
    image_bin = malloc(MAX_IMAGE_BINARY_SIZE);
    if (!image_bin)
    {
        ESP_LOGE(TAG, "Binary buffer alloc failed");
        return;
    }

    int ret = mbedtls_base64_decode(
        image_bin,
        MAX_IMAGE_BINARY_SIZE,
        &image_bin_len,
        (unsigned char *)image_b64,
        image_b64_len);

    if (ret != 0)
    {
        ESP_LOGE(TAG, "Base64 decode failed: %d", ret);
        return;
    }

    ESP_LOGI(TAG, "Image decoded successfully");
    ESP_LOGI(TAG, "Binary size: %d bytes", image_bin_len);

    // ---- PNG signature check ----
    const uint8_t png_magic[8] = {
        0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};

    if (memcmp(image_bin, png_magic, 8) == 0)
    {
        ESP_LOGI(TAG, "PNG signature OK");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid PNG signature");
    }
}

// ------------------------------------------------------------

static void handle_image_json(const char *payload)
{
    cJSON *root = cJSON_Parse(payload);
    if (!root)
    {
        ESP_LOGE(TAG, "Invalid JSON");
        return;
    }

    cJSON *data = cJSON_GetObjectItem(root, "data");
    cJSON *type = cJSON_GetObjectItem(root, "type");

    if (!cJSON_IsString(data) || !cJSON_IsString(type))
    {
        ESP_LOGE(TAG, "Malformed image JSON");
        cJSON_Delete(root);
        return;
    }

    if (strcmp(type->valuestring, "png") != 0)
    {
        ESP_LOGW(TAG, "Unexpected image type: %s", type->valuestring);
    }

    size_t chunk_len = strlen(data->valuestring);

    if (!image_b64)
    {
        image_b64 = malloc(MAX_IMAGE_BASE64_SIZE);
        image_b64_len = 0;
    }

    memcpy(image_b64 + image_b64_len, data->valuestring, chunk_len);
    image_b64_len += chunk_len;

    ESP_LOGI(TAG, "Received image chunk (%d bytes)", chunk_len);
    ESP_LOGI(TAG, "Total Base64 size: %d", image_b64_len);

    cJSON_Delete(root);
}

// ------------------------------------------------------------

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event_id)
    {

    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");

        esp_mqtt_client_subscribe(
            mqtt_client,
            TASK4_CHALLENGE_TOPIC,
            1);

        publish_task4_request();
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "RX | topic='%.*s' len=%d",
                 event->topic_len,
                 event->topic,
                 event->data_len);

        ESP_LOGI(TAG, "Payload: %.*s",
                 event->data_len,
                 event->data);

        // If JSON with "data", treat as image chunk
        if (strstr(event->data, "\"data\""))
        {
            handle_image_json(event->data);
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        break;

    default:
        break;
    }
}

// ------------------------------------------------------------

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    image_b64 = malloc(MAX_IMAGE_BASE64_SIZE);
    memset(image_b64, 0, MAX_IMAGE_BASE64_SIZE);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = MQTT_BROKER_URI};

    mqtt_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler,
        NULL);

    esp_mqtt_client_start(mqtt_client);

    // ---- Wait for all chunks ----
    vTaskDelay(pdMS_TO_TICKS(5000));

    ESP_LOGI(TAG, "Attempting image decode...");
    try_decode_image();

    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
