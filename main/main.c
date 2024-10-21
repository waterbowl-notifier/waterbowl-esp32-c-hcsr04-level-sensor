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

#include "certificate.h"
#include "key.h"

#define ORPHAN_TIMEOUT pdMS_TO_TICKS(7200000) // 2 hours in milliseconds

static const char *TAG = "WATER_BOWL";
const char *device_name = CONFIG_WIFI_HOSTNAME;

TaskHandle_t ota_task_handle = NULL; // Task handle for OTA updating
TimerHandle_t orphan_timer = NULL;

esp_mqtt_client_handle_t mqtt_client_handle = NULL;

extern unsigned char certificate[];
extern unsigned char key[];

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

    // msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, 0);
    // ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, msg_id);

    // msg_id =
    //     esp_mqtt_client_publish(client, CONFIG_MQTT_PUBLISH_STATUS_TOPIC, "{\"message\":\"status_request\"}", 0, 0, 0);
    // ESP_LOGI(TAG, "Published initial status request, msg_id=%d", msg_id);
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
    /*
        if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_STATUS_TOPIC, event->topic_len) == 0)
        {
            custom_handle_mqtt_event_subscribe(event);
        }
        else if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_SELF_TEST_SNOOPER_TOPIC, event->topic_len) == 0)
        {
            // Use the global mac_address variable to pass the MAC address to the self-test function
            custom_handle_mqtt_event_self_test(event, mac_address);
        }
        else if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_SNOOPER_TOPIC, event->topic_len) == 0)
        {
            // Use the global mac_address variable to pass the MAC address to the OTA function
            custom_handle_mqtt_event_ota(event, mac_address);
        }
        else
        {
            ESP_LOGE(TAG, "Un-Handled topic %.*s", event->topic_len, event->topic);
        }
    */
}

void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
    {
        ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    error_reload(mqtt_client_handle);
}

void app_main()
{
    init_nvs();

    init_wifi();

    show_mac_address();

    init_time_sync();

    mqtt_set_event_connected_handler(custom_handle_mqtt_event_connected);
    mqtt_set_event_disconnected_handler(custom_handle_mqtt_event_disconnected);
    mqtt_set_event_data_handler(custom_handle_mqtt_event_data);
    mqtt_set_event_error_handler(custom_handle_mqtt_event_error);

    mqtt_config_t config = {.certificate = (uint8_t *)certificate, .private_key = (uint8_t *)key, .broker_uri = CONFIG_IOT_ENDPOINT};

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

    hcsr04_init();

    while (true)
    {
        ESP_LOGI(TAG, "Distance: %.1f cm", hcsr04_get_range());
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
