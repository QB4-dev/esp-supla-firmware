#pragma once

#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>

#include "tuya-defs.h"
#include "tuya-dp.h"

typedef struct tuya_mcu *tuya_mcu_t;

enum tuya_mcu_state {
    TUYA_MCU_INIT_HEARTBEAT = 0x00,
    TUYA_MCU_QUERY_INFO = 0x01,
    TUYA_MCU_INITIALIZED = 0x02,
};

typedef int (*tuya_mcu_state_handler_t)(tuya_mcu_t mcu, enum tuya_mcu_state st, void *arg);
typedef int (*tuya_mcu_config_handler_t)(tuya_mcu_t mcu, void *arg);
typedef int (*tuya_mcu_dp_handler_t)(tuya_mcu_t mcu, tuya_dp_t *dp, void *arg);

int tuya_mcu_init(tuya_mcu_t *mcu, void *uart_ctx);
int tuya_mcu_deinit(tuya_mcu_t mcu);

char *tuya_mcu_get_product_id(tuya_mcu_t mcu);
char *tuya_mcu_get_version(tuya_mcu_t mcu);

int tuya_mcu_set_state_handler(tuya_mcu_t mcu, tuya_mcu_state_handler_t handler, void *arg);
int tuya_mcu_set_config_handler(tuya_mcu_t mcu, tuya_mcu_config_handler_t handler, void *arg);
int tuya_mcu_set_dp_handler(tuya_mcu_t mcu, tuya_mcu_dp_handler_t handler, void *arg);

int tuya_mcu_send_wifi_status(tuya_mcu_t mcu, uint8_t state);
int tuya_mcu_send_dp(tuya_mcu_t mcu, tuya_dp_t *dp);
int tuya_mcu_tick(tuya_mcu_t mcu);
