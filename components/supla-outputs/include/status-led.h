#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <esp_err.h>
#include <driver/gpio.h>

#include <libsupla/device.h>

typedef void *supla_status_led_t;

struct status_led_config {
    supla_dev_t *supla_dev;
    gpio_num_t   gpio;
    bool         inv_logic;
    bool         online_set;
};

supla_status_led_t supla_status_led_init(const struct status_led_config *config);

#ifdef __cplusplus
}
#endif
