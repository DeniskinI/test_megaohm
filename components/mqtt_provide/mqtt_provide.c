#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mb_m_provide.h"
#include "mqtt_provide.h"

static const char *TAG = "MQTTS_EXAMPLE";

static esp_mqtt_client_handle_t client = NULL;

static TaskHandle_t handle_mqtt_task = NULL;

static uint32_t set_poll_time = DEFAULT_POLL_INTERVAL / 10;


extern const uint8_t client_cert_pem_start[] asm("_binary_client_crt_start");
extern const uint8_t client_cert_pem_end[] asm("_binary_client_crt_end");
extern const uint8_t client_key_pem_start[] asm("_binary_client_key_start");
extern const uint8_t client_key_pem_end[] asm("_binary_client_key_end");
extern const uint8_t server_cert_pem_start[] asm("_binary_aws_crt_start");
extern const uint8_t server_cert_pem_end[] asm("_binary_aws_crt_end");

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}


void mqtt_send_task(void *pV) {
    int msg_id;
    int16_t hum, temp;
    char buf[32];
    uint32_t len_msg;
    uint32_t cnt_poll_time = 0;
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        if (++cnt_poll_time >= set_poll_time) {
            cnt_poll_time = 0;
            get_data_sens(&hum, &temp);
            memset(buf, 0, sizeof(buf));
            len_msg = sprintf(buf, "t = %.1f, h = %.1f", (float)temp, (float)hum);
            ESP_LOGI(TAG, "sent publish data: %s", buf);
            msg_id = esp_mqtt_client_publish(client, MQTT_TOPIC_PUBLISH, buf, len_msg, 2, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        }
    }
}

static void create_task_send_data(void) {
    xTaskCreate(mqtt_send_task, (const char*)"mqtt_task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY + 1, &handle_mqtt_task);
}

static void destroy_task_send_data(void) {
    vTaskDelete(handle_mqtt_task);
}


static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MQTT_TOPIC_SUBSCRABE, 2);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        create_task_send_data();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        destroy_task_send_data();
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        if (strstr(event->topic, MQTT_TOPIC_SUBSCRABE)) {
            int time_poll = atoi(event->data);
            if (time_poll >= MIN_TIME_POLL_INTERVAL && time_poll <= MAX_TIME_POLL_INTERVAL) {
                set_poll_time = time_poll / 10;
            }
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));

        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt_app_start(void)
{
    const esp_mqtt_client_config_t mqtt_cfg = {
        .uri = "mqtts://mqtt1.megaohm.ru:18883",
        .client_cert_pem = (const char *)client_cert_pem_start,
        .client_key_pem = (const char *)client_key_pem_start,
        .cert_pem = (const char *)server_cert_pem_start,
        .password = "RgsB8HPo6czi1sTcMVpz",
        .username = "1000000000200000000030D7"
    };
    // const esp_mqtt_client_config_t mqtt_cfg = {
    //     .uri = "mqtt://iot.beaconyun.com:1883",
    // };

    


    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void mqtt_task(void) {
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
