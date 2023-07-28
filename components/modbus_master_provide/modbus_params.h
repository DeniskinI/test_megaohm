#ifndef _DEVICE_PARAMS
#define _DEVICE_PARAMS

#include <stdint.h>

#define MB_PORT_NUM     1
#define MB_DEV_SPEED    9600

#define MB_ADDR_HUM_TEMP_SENS     3

#define CONFIG_MB_UART_RXD 22
#define CONFIG_MB_UART_TXD 23
#define CONFIG_MB_UART_RTS 18

#define POLL_TIMEOUT_MS                 (1000)
#define POLL_TIMEOUT_TICS               (POLL_TIMEOUT_MS / portTICK_RATE_MS)


#pragma pack(push, 1)
typedef struct
{
    uint16_t hum_val;
    uint16_t temp_val;

} holding_reg_params_t;
#pragma pack(pop)

extern holding_reg_params_t holding_reg_params;

#endif // !defined(_DEVICE_PARAMS)
