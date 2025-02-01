#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_err.h>
#include <driver/gpio.h>

#include <libsupla/device.h>

typedef void *supla_status_led_t;

supla_status_led_t supla_status_led_init(const supla_dev_t *dev, const gpio_num_t gpio);

#ifdef __cplusplus
}
#endif
