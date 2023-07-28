#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_log.h"
#include "esp_heap_caps.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/portable.h"
#include "freertos/queue.h"

#include "sdkconfig.h"

#include "mb_m_provide.h"

#include "mqtt_provide.h"


void app_main(void) {

    esp_log_level_set("*", ESP_LOG_INFO);

    mqtt_task();

    start_mb_task();

    while (1) {
        vTaskDelay(pdTICKS_TO_MS(1000));
    }
}
