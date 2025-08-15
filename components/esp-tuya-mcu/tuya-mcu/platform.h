#pragma once

#include <stdlib.h>
#include <inttypes.h>

int tuya_mcu_uart_rx(void *, uint8_t *c);
int tuya_mcu_uart_tx(void *, uint8_t c);
uint32_t tuya_mcu_get_tick(void);
