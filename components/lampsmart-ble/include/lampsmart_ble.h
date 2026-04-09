#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported LampSmart BLE protocol variants.
 */
typedef enum {
    LAMPSMART_VARIANT_3 = 0,
    LAMPSMART_VARIANT_2 = 1,
    LAMPSMART_VARIANT_1A = 2,
    LAMPSMART_VARIANT_1B = 3,
} lampsmart_variant_t;

/**
 * @brief LampSmart light controller configuration.
 */
typedef struct {
    lampsmart_variant_t variant;           /*!< LampSmart protocol variant */
    uint32_t            group_id;          /*!< Shared LampSmart group identifier */
    bool                reversed_channels; /*!< Swap cold and warm channels in outgoing commands */
    uint8_t             min_brightness;    /*!< Minimum non-zero brightness*/
    uint32_t            tx_duration_ms;    /*!< BLE advertisement duration  for each command */
} lampsmart_ble_config_t;

/**
 * @brief Opaque handle for one LampSmart light controller.
 *
 * Allocate with lampsmart_ble_init(); free with lampsmart_ble_deinit().
 */
typedef void *lampsmart_ble_t;

/** @brief Default LampSmart controller configuration initializer. */
#define LAMPSMART_BLE_CONFIG_DEFAULT()  \
    ((lampsmart_ble_config_t){          \
        .variant = LAMPSMART_VARIANT_3, \
        .group_id = 0xCAFEBABEU,        \
        .reversed_channels = false,     \
        .min_brightness = 8U,           \
        .tx_duration_ms = 800U,         \
    })

/**
 * @brief Initialize the BLE stack required by this component.
 *
 * This function configures the ESP-IDF BLE controller and Bluedroid stack.
 * Call nvs_flash_init() in the application before invoking this function.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_STATE if Bluetooth is disabled in sdkconfig
 *      - Other ESP-IDF error codes returned by the BLE stack
 */
esp_err_t lampsmart_ble_stack_init(void);

/**
 * @brief Allocate and initialize a LampSmart light controller.
 *
 * @param[out] out_light Set to a newly allocated controller handle on success.
 *                       Free with lampsmart_ble_deinit() when no longer needed.
 * @param[in]  cfg       Controller configuration.
 *
 * @return
 *      - ESP_OK on success
 *      - ESP_ERR_INVALID_ARG if any argument is NULL
 *      - ESP_ERR_NO_MEM if heap allocation fails
 *      - Other ESP-IDF error codes returned by lampsmart_ble_stack_init()
 */
esp_err_t lampsmart_ble_init(lampsmart_ble_t *out_light, const lampsmart_ble_config_t *cfg);

/**
 * @brief Free a LampSmart controller handle allocated by lampsmart_ble_init().
 *
 * @param[in] light Controller handle to free. May be NULL.
 */
void lampsmart_ble_deinit(lampsmart_ble_t light);

/**
 * @brief Send a LampSmart pair command.
 *
 * @param[in,out] light Initialized controller instance.
 *
 * @return ESP-IDF error code.
 */
esp_err_t lampsmart_ble_pair(lampsmart_ble_t light);

/**
 * @brief Send a LampSmart unpair command.
 *
 * @param[in,out] light Initialized controller instance.
 *
 * @return ESP-IDF error code.
 */
esp_err_t lampsmart_ble_unpair(lampsmart_ble_t light);

/**
 * @brief Send a LampSmart light-on command.
 *
 * @param[in,out] light Initialized controller instance.
 *
 * @return ESP-IDF error code.
 */
esp_err_t lampsmart_ble_turn_on(lampsmart_ble_t light);

/**
 * @brief Send a LampSmart light-off command.
 *
 * @param[in,out] light Initialized controller instance.
 *
 * @return ESP-IDF error code.
 */
esp_err_t lampsmart_ble_turn_off(lampsmart_ble_t light);

/**
 * @brief Set cold and warm white output levels.
 *
 * A `(0, 0)` level request is converted to an off command. For non-zero values,
 * the component automatically sends an on command first when the cached state
 * is off.
 *
 * @param[in,out] light Initialized controller instance.
 * @param[in] cold Cold white level in range 0..255.
 * @param[in] warm Warm white level in range 0..255.
 *
 * @return ESP-IDF error code.
 */
esp_err_t lampsmart_ble_set_levels(lampsmart_ble_t light, uint8_t cold, uint8_t warm);

#ifdef __cplusplus
}
#endif
