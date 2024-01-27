/*
 * board.h
 *
 *  Created on: 29 sie 2023
 *      Author: kuba
 */

#ifndef MAIN_BSP_BOARD_H_
#define MAIN_BSP_BOARD_H_

#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <esp-supla.h>
#include <driver/gpio.h>
#include "../device.h"

typedef struct {
    const char *id;
}bsp_t;

extern bsp_t * const bsp;

esp_err_t board_early_init(void);
esp_err_t board_init(supla_dev_t *dev);

#endif /* MAIN_BSP_BOARD_H_ */
