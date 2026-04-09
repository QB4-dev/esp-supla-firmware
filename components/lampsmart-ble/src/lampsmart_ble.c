#include "lampsmart_ble.h"

#include <stdlib.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "mbedtls/aes.h"

#define LAMPSMART_TAG "LAMPSMART_BLE"

#define LAMPSMART_CMD_PAIR 0x28
#define LAMPSMART_CMD_UNPAIR 0x45
#define LAMPSMART_CMD_TURN_ON 0x10
#define LAMPSMART_CMD_TURN_OFF 0x11
#define LAMPSMART_CMD_DIM 0x21

#define LAMPSMART_DEVICE_TYPE_LAMP 0x0100
#define LAMPSMART_MAX_PACKET_LEN 31

#define BIT_RAW_CFG_DONE BIT0
#define BIT_ADV_START BIT1
#define BIT_ADV_STOP BIT2

typedef struct {
    lampsmart_ble_config_t cfg;
    uint8_t                tx_count;
    bool                   is_off;
} lampsmart_ble_ctx_t;

typedef struct {
    lampsmart_variant_t variant;
    uint32_t            identifier;
    uint8_t             command;
    uint8_t             arg0;
    uint8_t             arg1;
    uint8_t             tx_count;
} lampsmart_packet_cmd_t;

#pragma pack(push, 1)
typedef union {
    struct {
        uint8_t  prefix[10];
        uint8_t  packet_num;
        uint16_t type;
        uint32_t group_id;
        uint8_t  group_index;
        uint8_t  command;
        uint8_t  reserved_19;
        uint16_t reserved_20;
        uint8_t  channel1;
        uint8_t  channel2;
        uint16_t signature_v3;
        uint8_t  reserved_26;
        uint16_t seed;
        uint16_t crc16;
    } fields;
    uint8_t raw[LAMPSMART_MAX_PACKET_LEN];
} lampsmart_adv_v3_t;

typedef union {
    struct {
        uint8_t  prefix[8];
        uint8_t  command;
        uint16_t group_index;
        uint8_t  arg0;
        uint8_t  arg1;
        uint8_t  arg2;
        uint8_t  tx_count;
        uint8_t  outs;
        uint8_t  src;
        uint8_t  reserved_2;
        uint16_t seed;
        uint16_t crc16;
    } fields;
    uint8_t raw[22];
} lampsmart_adv_v1_t;
#pragma pack(pop)

static EventGroupHandle_t s_ble_events;
static SemaphoreHandle_t  s_send_lock;
static bool               s_gap_registered;
static bool               s_ble_ready;

static lampsmart_ble_ctx_t *lampsmart_ctx(lampsmart_ble_t light)
{
    return (lampsmart_ble_ctx_t *)light;
}

static esp_ble_adv_params_t s_adv_params = {
    .adv_int_min = 0x20,
    .adv_int_max = 0x20,
    .adv_type = ADV_TYPE_NONCONN_IND,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .peer_addr = { 0x00 },
    .peer_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .channel_map = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

static const uint8_t s_xboxes[128] = {
    0xB7, 0xFD, 0x93, 0x26, 0x36, 0x3F, 0xF7, 0xCC, 0x34, 0xA5, 0xE5, 0xF1, 0x71, 0xD8, 0x31, 0x15,
    0x04, 0xC7, 0x23, 0xC3, 0x18, 0x96, 0x05, 0x9A, 0x07, 0x12, 0x80, 0xE2, 0xEB, 0x27, 0xB2, 0x75,
    0xD0, 0xEF, 0xAA, 0xFB, 0x43, 0x4D, 0x33, 0x85, 0x45, 0xF9, 0x02, 0x7F, 0x50, 0x3C, 0x9F, 0xA8,
    0x51, 0xA3, 0x40, 0x8F, 0x92, 0x9D, 0x38, 0xF5, 0xBC, 0xB6, 0xDA, 0x21, 0x10, 0xFF, 0xF3, 0xD2,
    0xE0, 0x32, 0x3A, 0x0A, 0x49, 0x06, 0x24, 0x5C, 0xC2, 0xD3, 0xAC, 0x62, 0x91, 0x95, 0xE4, 0x79,
    0xE7, 0xC8, 0x37, 0x6D, 0x8D, 0xD5, 0x4E, 0xA9, 0x6C, 0x56, 0xF4, 0xEA, 0x65, 0x7A, 0xAE, 0x08,
    0xE1, 0xF8, 0x98, 0x11, 0x69, 0xD9, 0x8E, 0x94, 0x9B, 0x1E, 0x87, 0xE9, 0xCE, 0x55, 0x28, 0xDF,
    0x8C, 0xA1, 0x89, 0x0D, 0xBF, 0xE6, 0x42, 0x68, 0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16,
};

static void lampsmart_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:
        if (param->adv_data_raw_cmpl.status == ESP_BT_STATUS_SUCCESS)
            xEventGroupSetBits(s_ble_events, BIT_RAW_CFG_DONE);
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS)
            xEventGroupSetBits(s_ble_events, BIT_ADV_START);
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status == ESP_BT_STATUS_SUCCESS)
            xEventGroupSetBits(s_ble_events, BIT_ADV_STOP);
        break;
    default:
        break;
    }
}

static uint16_t lampsmart_random_seed(void)
{
    return (uint16_t)(esp_random() & 0xFFFFU);
}

static uint16_t lampsmart_crc16_be(const uint8_t *data, size_t length, uint16_t seed)
{
    size_t   i;
    uint16_t crc = seed;

    for (i = 0; i < length; i++) {
        crc = (uint16_t)(crc ^ ((uint16_t)data[i] << 8));
        for (uint8_t bit = 0; bit < 8; bit++) {
            if ((crc & 0x8000U) == 0U)
                crc <<= 1;
            else
                crc = (uint16_t)((crc << 1) ^ 0x1021U);
        }
    }

    return crc;
}

static void lampsmart_v2_whiten(uint8_t *data, uint8_t length, uint8_t seed, uint8_t salt)
{
    for (uint8_t i = 0; i < length; i++) {
        uint8_t index = (uint8_t)(((seed + i + 9U) & 0x1FU) + ((salt & 0x03U) * 0x20U));
        data[i] ^= s_xboxes[index];
        data[i] ^= seed;
    }
}

static void lampsmart_ble_whiten(uint8_t *data, size_t start, size_t length, uint8_t seed)
{
    uint8_t lfsr = seed;

    for (size_t i = 0; i < start + length; i++) {
        uint8_t mask = 0;

        for (uint8_t bit = 0; bit < 8; bit++) {
            lfsr <<= 1;
            if ((lfsr & 0x80U) != 0U) {
                lfsr ^= 0x11U;
                mask |= (uint8_t)(1U << bit);
            }
            lfsr &= 0x7FU;
        }

        if (i >= start)
            data[i - start] ^= mask;
    }
}

static uint8_t lampsmart_reverse_byte(uint8_t value)
{
    value = (uint8_t)(((value & 0x55U) << 1) | ((value & 0xAAU) >> 1));
    value = (uint8_t)(((value & 0x33U) << 2) | ((value & 0xCCU) >> 2));
    value = (uint8_t)(((value & 0x0FU) << 4) | ((value & 0xF0U) >> 4));
    return value;
}

static uint16_t lampsmart_sign_v3(const uint8_t *payload, uint8_t tx_count, uint16_t seed)
{
    uint8_t             sigkey[16] = { 0,    0,    0,    0x0D, 0xBF, 0xE6, 0x42, 0x68,
                                       0x41, 0x99, 0x2D, 0x0F, 0xB0, 0x54, 0xBB, 0x16 };
    uint8_t             input[16];
    uint8_t             output[16];
    uint16_t            signature;
    mbedtls_aes_context aes_ctx;

    sigkey[0] = (uint8_t)(seed & 0xFFU);
    sigkey[1] = (uint8_t)((seed >> 8) & 0xFFU);
    sigkey[2] = tx_count;

    memcpy(input, payload, sizeof(input));

    mbedtls_aes_init(&aes_ctx);
    mbedtls_aes_setkey_enc(&aes_ctx, sigkey, sizeof(sigkey) * 8U);
    mbedtls_aes_crypt_ecb(&aes_ctx, MBEDTLS_AES_ENCRYPT, input, output);
    mbedtls_aes_free(&aes_ctx);

    memcpy(&signature, output, sizeof(signature));
    return (signature == 0U) ? 0xFFFFU : signature;
}

static size_t lampsmart_build_v3_packet(const lampsmart_packet_cmd_t *command, uint8_t *buffer)
{
    uint16_t            seed = lampsmart_random_seed();
    const uint8_t       header[] = { 0x02, 0x01, 0x02, 0x1B, 0x16, 0xF0, 0x08, 0x10, 0x80, 0x00 };
    lampsmart_adv_v3_t *pkt = (lampsmart_adv_v3_t *)buffer;

    memset(pkt, 0, sizeof(*pkt));
    memcpy(pkt->fields.prefix, header, sizeof(header));
    pkt->fields.packet_num = command->tx_count;
    pkt->fields.type = LAMPSMART_DEVICE_TYPE_LAMP;
    pkt->fields.group_id = command->identifier;
    pkt->fields.group_index = 0;
    pkt->fields.command = command->command;
    pkt->fields.channel1 = command->arg0;
    pkt->fields.channel2 = command->arg1;
    pkt->fields.seed = seed;

    if (command->variant == LAMPSMART_VARIANT_3)
        pkt->fields.signature_v3 = lampsmart_sign_v3(&pkt->raw[8], pkt->fields.packet_num, seed);

    lampsmart_v2_whiten(&pkt->raw[9], 0x12U, (uint8_t)seed, 0);
    pkt->fields.crc16 = lampsmart_crc16_be(&pkt->raw[7], 0x16U, (uint16_t)~seed);

    return sizeof(*pkt);
}

static size_t lampsmart_build_v1_base(const lampsmart_packet_cmd_t *command, uint8_t *buffer,
                                      size_t offset)
{
    uint16_t            seed = (uint16_t)(lampsmart_random_seed() % 65525U);
    const uint8_t       header[] = { 0xAA, 0x98, 0x43, 0xAF, 0x0B, 0x46, 0x46, 0x46 };
    uint16_t            group_index = (uint16_t)(command->identifier & 0xF0FFU);
    lampsmart_adv_v1_t *pkt = (lampsmart_adv_v1_t *)(buffer + offset);

    memset(pkt, 0, sizeof(*pkt));
    memcpy(pkt->fields.prefix, header, sizeof(header));
    pkt->fields.command = command->command;
    pkt->fields.group_index = group_index;
    pkt->fields.arg0 = command->arg0;
    pkt->fields.arg1 = command->arg1;
    pkt->fields.arg2 = 0;
    pkt->fields.tx_count = command->tx_count;
    pkt->fields.outs = 0;
    pkt->fields.src = (uint8_t)(seed ^ 1U);
    pkt->fields.reserved_2 = (uint8_t)(seed ^ 1U);
    pkt->fields.seed = htons(seed);

    if (command->command == LAMPSMART_CMD_PAIR) {
        pkt->fields.arg0 = (uint8_t)(group_index & 0xFFU);
        pkt->fields.arg1 = (uint8_t)(group_index >> 8);
        pkt->fields.arg2 = 0x81U;
    }

    pkt->fields.crc16 = htons(lampsmart_crc16_be(&pkt->raw[8], 12, (uint16_t)~seed));
    return sizeof(*pkt);
}

static size_t lampsmart_build_v1a_packet(const lampsmart_packet_cmd_t *command, uint8_t *buffer)
{
    static const uint8_t header[] = { 0x02, 0x01, 0x02, 0x1B, 0x03, 0x77, 0xF8 };
    const size_t         offset = sizeof(header);

    memcpy(buffer, header, sizeof(header));
    lampsmart_build_v1_base(command, buffer, offset);

    *(uint16_t *)&buffer[29] = htons(lampsmart_crc16_be(
        &buffer[offset + 8], 14, lampsmart_crc16_be(&buffer[offset + 1], 5, 0xFFFFU)));

    for (size_t i = offset; i < LAMPSMART_MAX_PACKET_LEN; i++) {
        buffer[i] = lampsmart_reverse_byte(buffer[i]);
    }

    lampsmart_ble_whiten(&buffer[offset], offset + 8U, LAMPSMART_MAX_PACKET_LEN - offset, 83U);
    return LAMPSMART_MAX_PACKET_LEN;
}

static size_t lampsmart_build_v1b_packet(const lampsmart_packet_cmd_t *command, uint8_t *buffer)
{
    static const uint8_t header[] = { 0x02, 0x01, 0x02, 0x1B, 0x03, 0xF9, 0x08, 0x49 };
    const size_t         offset = sizeof(header);

    memcpy(buffer, header, sizeof(header));
    lampsmart_build_v1_base(command, buffer, offset);
    buffer[30] = 0xAA;

    for (size_t i = offset; i < LAMPSMART_MAX_PACKET_LEN; i++) {
        buffer[i] = lampsmart_reverse_byte(buffer[i]);
    }

    lampsmart_ble_whiten(&buffer[offset], offset + 8U, LAMPSMART_MAX_PACKET_LEN - offset, 83U);
    return LAMPSMART_MAX_PACKET_LEN;
}

static size_t lampsmart_build_packet(const lampsmart_packet_cmd_t *command, uint8_t *buffer)
{
    switch (command->variant) {
    case LAMPSMART_VARIANT_3:
    case LAMPSMART_VARIANT_2:
        return lampsmart_build_v3_packet(command, buffer);
    case LAMPSMART_VARIANT_1A:
        return lampsmart_build_v1a_packet(command, buffer);
    case LAMPSMART_VARIANT_1B:
        return lampsmart_build_v1b_packet(command, buffer);
    default:
        return 0;
    }
}

static esp_err_t lampsmart_send_adv_packet(uint8_t *packet, size_t packet_len, uint32_t duration_ms)
{
    EventBits_t bits;
    esp_err_t   err;

    if (!s_ble_ready)
        return ESP_ERR_INVALID_STATE;

    xEventGroupClearBits(s_ble_events, BIT_RAW_CFG_DONE | BIT_ADV_START | BIT_ADV_STOP);

    err = esp_ble_gap_config_adv_data_raw(packet, (uint32_t)packet_len);
    if (err != ESP_OK) {
        ESP_LOGE(LAMPSMART_TAG, "esp_ble_gap_config_adv_data_raw failed: %s", esp_err_to_name(err));
        return err;
    }

    bits = xEventGroupWaitBits(s_ble_events, BIT_RAW_CFG_DONE, pdTRUE, pdTRUE, pdMS_TO_TICKS(500));
    if ((bits & BIT_RAW_CFG_DONE) == 0) {
        return ESP_ERR_TIMEOUT;
    }

    err = esp_ble_gap_start_advertising(&s_adv_params);
    if (err != ESP_OK) {
        ESP_LOGE(LAMPSMART_TAG, "esp_ble_gap_start_advertising failed: %s", esp_err_to_name(err));
        return err;
    }

    bits = xEventGroupWaitBits(s_ble_events, BIT_ADV_START, pdTRUE, pdTRUE, pdMS_TO_TICKS(500));
    if ((bits & BIT_ADV_START) == 0) {
        return ESP_ERR_TIMEOUT;
    }

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    err = esp_ble_gap_stop_advertising();
    if (err != ESP_OK) {
        ESP_LOGE(LAMPSMART_TAG, "esp_ble_gap_stop_advertising failed: %s", esp_err_to_name(err));
        return err;
    }
    xEventGroupWaitBits(s_ble_events, BIT_ADV_STOP, pdTRUE, pdTRUE, pdMS_TO_TICKS(500));
    return ESP_OK;
}

static esp_err_t lampsmart_send_command(lampsmart_ble_t light, uint8_t cmd, uint8_t cw, uint8_t ww)
{
    lampsmart_ble_ctx_t   *ctx = lampsmart_ctx(light);
    uint8_t                pkt[LAMPSMART_MAX_PACKET_LEN] = { 0 };
    size_t                 pkt_len;
    esp_err_t              rc;
    lampsmart_packet_cmd_t pkt_cmd;

    if (ctx == NULL)
        return ESP_ERR_INVALID_ARG;

    if (!s_ble_ready)
        return ESP_ERR_INVALID_STATE;

    if (xSemaphoreTake(s_send_lock, pdMS_TO_TICKS(1000)) != pdTRUE)
        return ESP_ERR_TIMEOUT;

    ctx->tx_count++;
    if (ctx->tx_count == 0U)
        ctx->tx_count = 1U;

    pkt_cmd.variant = ctx->cfg.variant;
    pkt_cmd.identifier = ctx->cfg.group_id;
    pkt_cmd.command = cmd;
    pkt_cmd.arg0 = ctx->cfg.reversed_channels ? ww : cw;
    pkt_cmd.arg1 = ctx->cfg.reversed_channels ? cw : ww;
    pkt_cmd.tx_count = ctx->tx_count;

    pkt_len = lampsmart_build_packet(&pkt_cmd, pkt);
    if (pkt_len > 0U)
        rc = lampsmart_send_adv_packet(pkt, pkt_len, ctx->cfg.tx_duration_ms);
    else
        rc = ESP_FAIL;

    xSemaphoreGive(s_send_lock);
    return rc;
}

esp_err_t lampsmart_ble_stack_init(void)
{
    esp_err_t err;

    if (s_ble_ready) {
        return ESP_OK;
    }

#if !defined(CONFIG_BT_ENABLED) || !CONFIG_BT_ENABLED || !defined(CONFIG_BT_BLE_ENABLED) || \
    !CONFIG_BT_BLE_ENABLED || !defined(CONFIG_BT_BLUEDROID_ENABLED) ||                      \
    !CONFIG_BT_BLUEDROID_ENABLED
    ESP_LOGE(LAMPSMART_TAG, "Bluetooth/BLE/Bluedroid is disabled in sdkconfig. "
                            "Enable CONFIG_BT_ENABLED, CONFIG_BT_BLE_ENABLED and "
                            "CONFIG_BT_BLUEDROID_ENABLED.");
    return ESP_ERR_INVALID_STATE;
#else
    if (s_ble_events == NULL) {
        s_ble_events = xEventGroupCreate();
        if (s_ble_events == NULL)
            return ESP_ERR_NO_MEM;
    }

    if (s_send_lock == NULL) {
        s_send_lock = xSemaphoreCreateMutex();
        if (s_send_lock == NULL)
            return ESP_ERR_NO_MEM;
    }

    err = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    err = esp_bt_controller_init(&bt_cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    err = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    err = esp_bluedroid_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    err = esp_bluedroid_enable();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        return err;

    if (!s_gap_registered) {
        err = esp_ble_gap_register_callback(lampsmart_gap_cb);
        if (err != ESP_OK)
            return err;

        s_gap_registered = true;
    }

    s_ble_ready = true;
    return ESP_OK;
#endif
}

esp_err_t lampsmart_ble_init(lampsmart_ble_t *out_light, const lampsmart_ble_config_t *config)
{
    if (out_light == NULL || config == NULL)
        return ESP_ERR_INVALID_ARG;

    lampsmart_ble_ctx_t *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL)
        return ESP_ERR_NO_MEM;

    ctx->cfg = *config;
    ctx->is_off = true;

    esp_err_t err = lampsmart_ble_stack_init();
    if (err != ESP_OK) {
        free(ctx);
        return err;
    }

    *out_light = (lampsmart_ble_t)ctx;
    return ESP_OK;
}

void lampsmart_ble_deinit(lampsmart_ble_t light)
{
    free(light);
}

esp_err_t lampsmart_ble_pair(lampsmart_ble_t light)
{
    return lampsmart_send_command(light, LAMPSMART_CMD_PAIR, 0, 0);
}

esp_err_t lampsmart_ble_unpair(lampsmart_ble_t light)
{
    return lampsmart_send_command(light, LAMPSMART_CMD_UNPAIR, 0, 0);
}

esp_err_t lampsmart_ble_turn_on(lampsmart_ble_t light)
{
    lampsmart_ble_ctx_t *ctx = lampsmart_ctx(light);
    esp_err_t            err;

    if (ctx == NULL)
        return ESP_ERR_INVALID_ARG;

    err = lampsmart_send_command(light, LAMPSMART_CMD_TURN_ON, 0, 0);
    if (err == ESP_OK)
        ctx->is_off = false;

    return err;
}

esp_err_t lampsmart_ble_turn_off(lampsmart_ble_t light)
{
    lampsmart_ble_ctx_t *ctx = lampsmart_ctx(light);
    esp_err_t            err;

    if (ctx == NULL)
        return ESP_ERR_INVALID_ARG;

    err = lampsmart_send_command(light, LAMPSMART_CMD_TURN_OFF, 0, 0);
    if (err == ESP_OK)
        ctx->is_off = true;

    return err;
}

esp_err_t lampsmart_ble_set_levels(lampsmart_ble_t light, uint8_t cold, uint8_t warm)
{
    lampsmart_ble_ctx_t *ctx = lampsmart_ctx(light);
    esp_err_t            err;

    if (ctx == NULL)
        return ESP_ERR_INVALID_ARG;

    if (cold == 0U && warm == 0U)
        return lampsmart_ble_turn_off(light);

    if (cold < ctx->cfg.min_brightness && warm < ctx->cfg.min_brightness) {
        if (cold > 0U)
            cold = ctx->cfg.min_brightness;

        if (warm > 0U)
            warm = ctx->cfg.min_brightness;
    }

    if (ctx->is_off) {
        err = lampsmart_ble_turn_on(light);
        if (err != ESP_OK)
            return err;
    }

    err = lampsmart_send_command(light, LAMPSMART_CMD_DIM, cold, warm);
    if (err == ESP_OK)
        ctx->is_off = false;

    return err;
}