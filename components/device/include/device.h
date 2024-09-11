/*
 * device.h
 *
 *  Created on: 27 sty 2024
 *      Author: kuba
 */

#ifndef MAIN_DEVICE_H_
#define MAIN_DEVICE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>

#define DEVICE_CONFIG_EVENT_BIT BIT0

/**
 * @brief Declare device event base
 *
 */
ESP_EVENT_DECLARE_BASE(DEV_EVENT);

/**
 * @brief Device Event Event ID
 *
 */
typedef enum {
    DEVICE_EVENT_CONFIG_INIT, /*!< config mode init event*/
    DEVICE_EVENT_CONFIG_EXIT, /*!< config mode exit event*/
} device_event_id_t;

esp_err_t   device_init(esp_event_handler_t ev_hdl, void *args);
EventBits_t device_get_event_bits(void);

esp_err_t device_init_config(void);
esp_err_t device_exit_config(void);

#endif /* MAIN_DEVICE_H_ */
