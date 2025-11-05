#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <esp_types.h>
#include <esp_event.h>
#include <esp_err.h>
#include <driver/uart.h>
#include <driver/gpio.h>

#include "tuya-mcu.h"
#include "tuya-dp.h"

/**
 * @brief Declare of TUYA MCU Event base
 *
 */
ESP_EVENT_DECLARE_BASE(TUYA_MCU_EVENT);
/**
 * @brief TUYA MCU UART configuration structure
 *
 */
typedef struct {
    struct {
        uart_port_t        uart_port;        /*!< UART port number */
        uint32_t           rx_pin;           /*!< UART Rx Pin number */
        uint32_t           tx_pin;           /*!< UART Tx Pin number */
        uint32_t           baud_rate;        /*!< UART baud rate */
        uart_word_length_t data_bits;        /*!< UART data bits length */
        uart_parity_t      parity;           /*!< UART parity */
        uart_stop_bits_t   stop_bits;        /*!< UART stop bits length */
        uint32_t           event_queue_size; /*!< UART event queue size */
    } uart;                                  /*!< UART specific configuration */
} tuya_mcu_uart_config_t;

typedef void *esp_tuya_mcu_handle_t;

#if CONFIG_IDF_TARGET_ESP8266
#define TUYA_MCU_CONFIG_DEFAULT()              \
    { .uart = { .uart_port = UART_NUM_0,       \
                .baud_rate = 9600,             \
                .data_bits = UART_DATA_8_BITS, \
                .parity = UART_PARITY_DISABLE, \
                .stop_bits = UART_STOP_BITS_1, \
                .event_queue_size = 16 } }

#else
#define TUYA_MCU_CONFIG_DEFAULT()              \
    { .uart = { .uart_port = UART_NUM_1,       \
                .rx_pin = GPIO_NUM_23,         \
                .tx_pin = GPIO_NUM_22,         \
                .baud_rate = 9600,             \
                .data_bits = UART_DATA_8_BITS, \
                .parity = UART_PARITY_DISABLE, \
                .stop_bits = UART_STOP_BITS_1, \
                .event_queue_size = 16 } }
#endif

typedef enum {
    TUYA_MCU_EVENT_STATE_CHANGED = 0,
    TUYA_MCU_EVENT_CONFIG_REQUEST,
    TUYA_MCU_EVENT_DP_UPDATE,
} tuya_mcu_event_id_t;

/**
 * @brief Initialize TUYA MCU
 *
 * @param config Configuration for TUYA MCU
 * @return esp_tuya_mcu_handle_t Handle of TUYA MCU on success, NULL on error
 */
esp_tuya_mcu_handle_t esp_tuya_mcu_init(const tuya_mcu_uart_config_t *config);

/**
 * @brief Deinit TUYA MCU 
 * @param mcu_hdl handle of TUYA MCU
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_tuya_mcu_deinit(esp_tuya_mcu_handle_t mcu_hdl);

/**
 * @brief Add event handler for TUYA MCU
 *
 * @param mcu_hdl handle of TUYA MCU
 * @param handler Event handler function
 * @param arg Argument to pass to the handler
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_tuya_mcu_add_handler(esp_tuya_mcu_handle_t mcu_hdl, esp_event_handler_t handler,
                                   void *arg);

/**
 * @brief Remove event handler for TUYA MCU
 *
 * @param mcu_hdl handle of TUYA MCU
 * @param handler Event handler function to remove
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_tuya_mcu_remove_handler(esp_tuya_mcu_handle_t mcu_hdl, esp_event_handler_t handler);

/**
 * @brief Send WiFi state to TUYA MCU
 *
 * @param mcu_hdl handle of TUYA MCU
 * @param state WiFi state from WIFI work status in TUYA protocol
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_tuya_mcu_write_wifi_status(esp_tuya_mcu_handle_t mcu_hdl, uint8_t state);

/**
 * @brief Send data point to TUYA MCU
 *
 * @param mcu_hdl handle of TUYA MCU
 * @param dp Data point to send
 * @return esp_err_t ESP_OK on success, ESP_FAIL on error
 */
esp_err_t esp_tuya_mcu_write_dp(esp_tuya_mcu_handle_t mcu_hdl, tuya_dp_t *dp);

#ifdef __cplusplus
}
#endif
