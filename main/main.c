/*
ToDo:
- Get OTA working
- Get MQTT working
- Get RGB LED working
- Get periodic distance measurement working
*/
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "cJSON.h"

#include "gecl-mqtt-manager.h"
#include "gecl-nvs-manager.h"
#include "gecl-ota-manager.h"
#include "gecl-rgb-led-manager.h"
#include "gecl-time-sync-manager.h"
#include "gecl-wifi-manager.h"
#include "gecl-misc-util-manager.h"
#include "gecl-ultrasonic-manager.h"

#define ORPHAN_TIMEOUT pdMS_TO_TICKS(7200000) // 2 hours in milliseconds

static const char *TAG = "WATER_BOWL";
const char *device_name = CONFIG_WIFI_HOSTNAME;

TaskHandle_t ota_task_handle = NULL; // Task handle for OTA updating
TimerHandle_t orphan_timer = NULL;

esp_mqtt_client_handle_t mqtt_client_handle = NULL;

char mac_address[18];

extern const uint8_t certificate[];
extern const uint8_t key[];
extern const uint8_t AmazonRootCA1_pem[];
const uint8_t *my_cert = certificate;
const uint8_t *my_key = key;
const uint8_t *my_root_ca = AmazonRootCA1_pem;

// Callback function for timer expiration
void orphan_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGE(TAG, "No status message received for 2 hours. Triggering reboot!");
    error_reload(mqtt_client_handle);
}

// Function to reset the timer whenever a message is received
void reset_orphan_timer(void)
{
    if (xTimerReset(orphan_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Orphan timer failed to reset");
        error_reload(mqtt_client_handle);
    }
    else
    {
        ESP_LOGI(TAG, "Orphan timer reset successfully");
    }
}

void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_CONNECTED");
    int msg_id;

    msg_id = esp_mqtt_client_subscribe(client, CONFIG_SUBSCRIBE_LED_COLOR_TOPIC, 0);
    ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_SUBSCRIBE_LED_COLOR_TOPIC, msg_id);

    msg_id = esp_mqtt_client_subscribe(client, CONFIG_SUBSCRIBE_OTA_RELOAD_TOPIC, 0);
    ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_SUBSCRIBE_OTA_RELOAD_TOPIC, msg_id);
}

void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DISCONNECTED");
    if (ota_task_handle != NULL)
    {
        vTaskDelete(ota_task_handle);
        ota_task_handle = NULL;
    }
    // Reconnect logic
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 5000;
    esp_err_t err;
    esp_mqtt_client_handle_t client = event->client;

    // Check if the network is connected before attempting reconnection
    if (wifi_active())
    {
        do
        {
            ESP_LOGI(TAG, "Attempting to reconnect, retry %d/%d", retry_count + 1, max_retries);
            err = esp_mqtt_client_reconnect(client);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to reconnect MQTT client, retrying in %d seconds...", retry_delay_ms / 1000);
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms)); // Delay for 5 seconds
                retry_count++;
            }
        } while (err != ESP_OK && retry_count < max_retries);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to reconnect MQTT client after %d retries. Restarting", retry_count);
            error_reload(mqtt_client_handle);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Network not connected, skipping MQTT reconnection");
        error_reload(mqtt_client_handle);
    }
}

void custom_handle_mqtt_event_subscribe(esp_mqtt_event_handle_t event)
{
    // Handle the status response
    cJSON *json = cJSON_Parse(event->data);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON");
        error_reload(mqtt_client_handle);
    }
    else
    {
        cJSON *state = cJSON_GetObjectItem(json, "LED");
        const char *led_state = cJSON_GetStringValue(state);
        assert(led_state != NULL);
        set_rgb_led_named_color(led_state);
        cJSON_Delete(json);
    }
}

bool extract_ota_url_from_event(esp_mqtt_event_handle_t event, char *local_mac_address, char *ota_url)
{
    bool success = false;
    cJSON *root = cJSON_Parse(event->data);
    cJSON *host_key = cJSON_GetObjectItem(root, local_mac_address);
    const char *host_key_value = cJSON_GetStringValue(host_key);

    if (!host_key || !host_key_value)
    {
        ESP_LOGW(TAG, "'%s' MAC address key not found in JSON", local_mac_address);
    }
    else
    {
        size_t url_len = strlen(host_key_value);
        strncpy(ota_url, host_key_value, url_len);
        ota_url[url_len] = '\0'; // Manually set the null terminator
        success = true;
    }

    cJSON_Delete(root); // Free JSON object
    return success;
}

void custom_handle_mqtt_event_ota(esp_mqtt_event_handle_t event, char *my_mac_address)
{
    if (ota_task_handle != NULL)
    {
        eTaskState task_state = eTaskGetState(ota_task_handle);
        if (task_state != eDeleted)
        {
            char log_message[256]; // Adjust the size according to your needs
            snprintf(log_message, sizeof(log_message),
                     "OTA task is already running or not yet cleaned up, skipping OTA update. task_state=%d",
                     task_state);

            ESP_LOGW(TAG, "%s", log_message);
            return;
        }
        else
        {
            // Clean up task handle if it has been deleted
            ota_task_handle = NULL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "OTA task handle is NULL");
    }

    // Parse the message and get any URL associated with our MAC address
    assert(event->data != NULL);
    assert(event->data_len > 0);

    ota_config_t ota_config;
    ota_config.mqtt_client = event->client;

    if (!extract_ota_url_from_event(event, my_mac_address, ota_config.url))
    {
        ESP_LOGW(TAG, "OTA URL not found in event data");
        return;
    }

    set_rgb_led_named_color("LED_BLINK_GREEN");

    // Pass the allocated URL string to the OTA task
    if (xTaskCreate(&ota_task, "ota_task", 8192, (void *)&ota_config, 5, &ota_task_handle) != pdPASS)
    {
        error_reload(mqtt_client_handle);
    }
}

void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event)
{

    ESP_LOGW(TAG, "Received topic %.*s", event->topic_len, event->topic);

    // Reset the orphan timer whenever a message is received
    reset_orphan_timer();
    if (strncmp(event->topic, CONFIG_SUBSCRIBE_LED_COLOR_TOPIC, event->topic_len) == 0)
    {
        custom_handle_mqtt_event_subscribe(event);
    }
    else if (strncmp(event->topic, CONFIG_SUBSCRIBE_OTA_RELOAD_TOPIC, event->topic_len) == 0)
    {
        // Use the global mac_address variable to pass the MAC address to the OTA function
        custom_handle_mqtt_event_ota(event, mac_address);
    }
    else
    {
        ESP_LOGE(TAG, "Un-Handled topic %.*s", event->topic_len, event->topic);
    }
}

void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event)
{
    ESP_LOGE(TAG, "Custom handler: MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
    {
        ESP_LOGE(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGE(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGE(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
        ESP_LOGE(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
        ESP_LOGE(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    error_reload(mqtt_client_handle);
}

void ultrasonic_read_task(void *pvParameter)
{
    while (1)
    {
        // Ensure sensor is initialized
        if (!hcsr04_is_initialized())
        {
            hcsr04_init();
        }

        // Read the range in cm
        float distance = hcsr04_get_range();

        // Prepare the payload (you can format it however you like)
        char payload[100];
        snprintf(payload, sizeof(payload), "{\"water_level\": \"%.1f\", \"hostname\": \"%s\" }", distance, CONFIG_LOCATION);

        // Publish to MQTT topic
        int msg_id = esp_mqtt_client_publish(mqtt_client_handle, CONFIG_PUBLISH_WATER_LEVEL_TOPIC, payload, 0, 1, 0);
        if (msg_id == -1)
        {
            ESP_LOGE("MQTT", "Failed to publish message");
        }
        else
        {
            ESP_LOGI("MQTT", "Published message: %s", payload);
        }

        // Delay for configured time interval (convert minutes to ticks)
        vTaskDelay(CONFIG_WATER_LEVEL_CHECK_INTERVAL_MIN * 60 * 1000 / portTICK_PERIOD_MS);
    }
}

void app_main()
{
    init_nvs();

    init_wifi();

    get_device_mac_address(mac_address);
    ESP_LOGI(TAG, "MAC Address: %s", mac_address);

    init_time_sync();

    mqtt_set_event_connected_handler(custom_handle_mqtt_event_connected);
    mqtt_set_event_disconnected_handler(custom_handle_mqtt_event_disconnected);
    mqtt_set_event_data_handler(custom_handle_mqtt_event_data);
    mqtt_set_event_error_handler(custom_handle_mqtt_event_error);

    // ESP_LOGI(TAG, "Cert size: %d, Key size: %d", sizeof(certificate), sizeof(key));

    mqtt_config_t config = {
        .root_ca = my_root_ca,
        .certificate = my_cert,
        .private_key = my_key,
        .broker_uri = CONFIG_IOT_ENDPOINT};

    mqtt_client_handle = init_mqtt(&config);

    init_rgb_led();

    set_rgb_led_named_color("LED_BLINK_WHITE");
    // Create an orphan timer to trigger a notification if no message is received for 2 hours
    orphan_timer = xTimerCreate("orphan_timer", ORPHAN_TIMEOUT, pdFALSE, (void *)0, orphan_timer_callback);

    if (orphan_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create notification timer");
        error_reload(mqtt_client_handle);
    }

    // Start the timer when the system boots
    if (xTimerStart(orphan_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start notification timer");
        error_reload(mqtt_client_handle);
    }

    xTaskCreate(&ultrasonic_read_task, "ultrasonic_read_task", 4096, NULL, 5, NULL);

    while (true)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
