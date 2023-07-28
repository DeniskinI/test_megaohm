#include "mb_m_provide.h"

#include "string.h"
#include "esp_log.h"
#include "modbus_params.h"
#include "mbcontroller.h"
#include "sdkconfig.h"


#define MASTER_MAX_CIDS num_device_parameters

#define HOLD_OFFSET(field) ((uint16_t)(offsetof(holding_reg_params_t, field) + 1))

#define STR(fieldname) ((const char*)( fieldname ))

#define OPTS(min_val, max_val, step_val) { .opt1 = min_val, .opt2 = max_val, .opt3 = step_val }

static const char *TAG = "MASTER_TEST";

enum {
    MB_DEVICE_ADDR1 = MB_ADDR_HUM_TEMP_SENS,
};

enum {
    CID_HUM_TEMP_SENS = 0,
};

holding_reg_params_t holding_reg_params;

const mb_parameter_descriptor_t device_parameters[] = {
    { CID_HUM_TEMP_SENS, STR("HumTempSens"), STR(""), MB_DEVICE_ADDR1, MB_PARAM_HOLDING, 0, 2, HOLD_OFFSET(hum_val), PARAM_TYPE_U16, 4, OPTS( 0, 0, 0 ), PAR_PERMS_READ_WRITE_TRIGGER },
};

const uint16_t num_device_parameters = (sizeof(device_parameters)/sizeof(device_parameters[0]));


static void* master_get_param_data(const mb_parameter_descriptor_t* param_descriptor)
{
    assert(param_descriptor != NULL);
    void* instance_ptr = NULL;
    if (param_descriptor->param_offset != 0) {
       switch(param_descriptor->mb_param_type)
       {
           case MB_PARAM_HOLDING:
               instance_ptr = ((void*)&holding_reg_params + param_descriptor->param_offset - 1);
               break;
           default:
               instance_ptr = NULL;
               break;
       }
    } else {
        ESP_LOGE(TAG, "Wrong parameter offset for CID #%d", param_descriptor->cid);
        assert(instance_ptr != NULL);
    }
    return instance_ptr;
}


static void master_operation_func(void *arg)
{
    esp_err_t err = ESP_OK;
    uint32_t value = 0;
    const mb_parameter_descriptor_t* param_descriptor = NULL;

    ESP_LOGI(TAG, "Start modbus...");

    while(1) {
        for (uint16_t cid = 0; (err != ESP_ERR_NOT_FOUND) && cid < MASTER_MAX_CIDS; cid++)
        {
            err = mbc_master_get_cid_info(cid, &param_descriptor);
            if ((err != ESP_ERR_NOT_FOUND) && (param_descriptor != NULL)) {
                void* temp_data_ptr = master_get_param_data(param_descriptor);
                assert(temp_data_ptr);
                uint8_t type = 0;
                if ((param_descriptor->param_type == PARAM_TYPE_U16) && (param_descriptor->cid == CID_HUM_TEMP_SENS)) {
                    err = mbc_master_get_parameter(cid, (char*)param_descriptor->param_key, (uint8_t*)&value, &type);
                    if (err == ESP_OK) {
                        *(uint32_t*)temp_data_ptr = value;

                        if ((param_descriptor->mb_param_type == MB_PARAM_HOLDING) || (param_descriptor->mb_param_type == MB_PARAM_INPUT)) {
                            ESP_LOGI(TAG, "%d, %d", (int16_t)holding_reg_params.hum_val, (int16_t)holding_reg_params.temp_val);
                        }
                    } else {
                        ESP_LOGE(TAG, "Characteristic #%d (%s) read fail, err = 0x%x (%s).",
                                            param_descriptor->cid,
                                            (char*)param_descriptor->param_key,
                                            (int)err,
                                            (char*)esp_err_to_name(err));
                    }
                }
            }
        }
        vTaskDelay(POLL_TIMEOUT_TICS);
    }
}


static esp_err_t master_init(void) {
    // Initialize and start Modbus controller
    mb_communication_info_t comm = {
            .port = MB_PORT_NUM,
            .mode = MB_MODE_RTU,
            .baudrate = MB_DEV_SPEED,
            .parity = MB_PARITY_NONE
    };
    void* master_handler = NULL;

    esp_err_t err = mbc_master_init(MB_PORT_SERIAL_MASTER, &master_handler);
    MB_RETURN_ON_FALSE((master_handler != NULL), ESP_ERR_INVALID_STATE, TAG, "mb controller initialization fail.");
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller initialization fail, returns(0x%x).", (uint32_t)err);
    err = mbc_master_setup((void*)&comm);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller setup fail, returns(0x%x).", (uint32_t)err);

    // Set UART pin numbers
    err = uart_set_pin(MB_PORT_NUM, CONFIG_MB_UART_TXD, CONFIG_MB_UART_RXD, CONFIG_MB_UART_RTS, UART_PIN_NO_CHANGE);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb serial set pin failure, uart_set_pin() returned (0x%x).", (uint32_t)err);

    err = mbc_master_start();
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller start fail, returns(0x%x).", (uint32_t)err);

    // Set driver mode to Half Duplex
    err = uart_set_mode(MB_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb serial set mode failure, uart_set_mode() returned (0x%x).", (uint32_t)err);

    vTaskDelay(5);
    err = mbc_master_set_descriptor(&device_parameters[0], num_device_parameters);
    MB_RETURN_ON_FALSE((err == ESP_OK), ESP_ERR_INVALID_STATE, TAG, "mb controller set descriptor fail, returns(0x%x).", (uint32_t)err);
    ESP_LOGI(TAG, "Modbus master stack initialized...");
    return err;
}

void get_data_sens(int16_t *hum, int16_t *temp) {
    *hum = (int16_t)holding_reg_params.hum_val;
    *temp = (int16_t)holding_reg_params.temp_val;
}

void mb_task(void *pv) {
    ESP_ERROR_CHECK(master_init());
    vTaskDelay(10);

    master_operation_func(NULL);
}

void start_mb_task(void) {
    xTaskCreate(mb_task, (const char*)"mb_task", configMINIMAL_STACK_SIZE * 4, NULL, tskIDLE_PRIORITY, NULL);
}
