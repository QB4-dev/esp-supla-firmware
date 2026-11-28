#ifndef RELAY_H_
#define RELAY_H_

#include <stdbool.h>
#include <driver/gpio.h>

struct relay_config {
    gpio_num_t gpio_num;
    bool       active_high;
};

typedef void *relay_handle_t;

relay_handle_t relay_create(const struct relay_config *config);
esp_err_t      relay_delete(relay_handle_t relay);

esp_err_t relay_on(relay_handle_t relay);
esp_err_t relay_off(relay_handle_t relay);
esp_err_t relay_toggle(relay_handle_t relay);
esp_err_t relay_is_on(relay_handle_t handle, bool *is_on);

#endif // RELAY_H_
