/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef MAIN_DEVICE_H_
#define MAIN_DEVICE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_err.h>

/**
 * @brief Declare device event base
 *
 */
ESP_EVENT_DECLARE_BASE(DEV_EVENT);

#define DEVICE_CONFIG_EVENT_BIT BIT0

/**
 * @brief Device Event Event ID
 *
 */
typedef enum {
    DEVICE_EVENT_CONFIG_INIT,  /*!< config mode init event*/
    DEVICE_EVENT_CONFIG_EXIT,  /*!< config mode exit event*/
    DEVICE_EVENT_SLEEP_INIT,   /*!< sleep mode init event*/
    DEVICE_EVENT_SLEEP_RESUME, /*!< sleep mode exit event*/
} device_event_id_t;

esp_err_t   device_init(esp_event_handler_t ev_hdl, void *args);
EventBits_t device_get_event_bits(void);

esp_err_t device_init_config(void);
esp_err_t device_exit_config(void);

#endif /* MAIN_DEVICE_H_ */
