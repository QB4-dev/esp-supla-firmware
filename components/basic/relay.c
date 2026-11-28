#include "include/relay.h"
#include <stdlib.h>

#define CHECK_ARG(VAL)                  \
    do {                                \
        if (!(VAL))                     \
            return ESP_ERR_INVALID_ARG; \
    } while (0)

typedef struct {
    gpio_num_t gpio;
    bool       active_high;
} relay_t;

relay_handle_t relay_create(const struct relay_config *config)
{
    if (!config)
        return NULL;

    relay_t *relay = calloc(1, sizeof(relay_t));
    if (!relay)
        return NULL;

    relay->gpio = config->gpio_num;
    relay->active_high = config->active_high;

    gpio_config_t io_conf = { .pin_bit_mask = 1ULL << relay->gpio,
                              .mode = GPIO_MODE_OUTPUT,
                              .pull_up_en = 0,
                              .pull_down_en = 0,
                              .intr_type = GPIO_INTR_DISABLE };
    gpio_config(&io_conf);
    relay_off(relay);
    return (relay_handle_t)relay;
}

esp_err_t relay_delete(relay_handle_t handle)
{
    CHECK_ARG(handle);
    relay_t *relay = (relay_t *)handle;

    relay_off(relay);
    free(relay);
    return ESP_OK;
}

esp_err_t relay_on(relay_handle_t handle)
{
    CHECK_ARG(handle);
    relay_t *relay = (relay_t *)handle;

    if (gpio_set_level(relay->gpio, relay->active_high ? 1 : 0) != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t relay_off(relay_handle_t handle)
{
    CHECK_ARG(handle);
    relay_t *relay = (relay_t *)handle;

    if (gpio_set_level(relay->gpio, relay->active_high ? 0 : 1) != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t relay_toggle(relay_handle_t handle)
{
    CHECK_ARG(handle);
    relay_t *relay = (relay_t *)handle;

    int level = gpio_get_level(relay->gpio);
    if (gpio_set_level(relay->gpio, !level) != ESP_OK)
        return ESP_FAIL;
    return ESP_OK;
}

esp_err_t relay_is_on(relay_handle_t handle, bool *is_on)
{
    CHECK_ARG(handle && is_on);
    relay_t *relay = (relay_t *)handle;

    int level = gpio_get_level(relay->gpio);
    *is_on = relay->active_high ? (level == 1) : (level == 0);
    return ESP_OK;
}