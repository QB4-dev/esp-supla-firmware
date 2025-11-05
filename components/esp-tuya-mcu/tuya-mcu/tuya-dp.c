#include "tuya-dp.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

//-----------------------------
// Setter functions
//-----------------------------
void tuya_dp_set_raw(tuya_dp_t *dp, uint8_t id, const uint8_t *buf, uint16_t len)
{
    dp->id = id;
    dp->type = DP_TYPE_RAW;
    dp->len = (len <= sizeof(dp->data.raw)) ? len : sizeof(dp->data.raw);
    memcpy(dp->data.raw, buf, dp->len);
}

void tuya_dp_set_bool(tuya_dp_t *dp, uint8_t id, bool value)
{
    dp->id = id;
    dp->type = DP_TYPE_BOOL;
    dp->len = 1;
    dp->data.boolean = value;
    dp->data.raw[0] = value ? 1 : 0; // For consistency with Tuya MCU format
}

void tuya_dp_set_value(tuya_dp_t *dp, uint8_t id, int32_t value)
{
    dp->id = id;
    dp->type = DP_TYPE_VALUE;
    dp->len = 4;
    dp->data.value = value;
    // Store big-endian representation in raw
    dp->data.raw[0] = (uint8_t)((value >> 24) & 0xFF);
    dp->data.raw[1] = (uint8_t)((value >> 16) & 0xFF);
    dp->data.raw[2] = (uint8_t)((value >> 8) & 0xFF);
    dp->data.raw[3] = (uint8_t)(value & 0xFF);
}

void tuya_dp_set_string(tuya_dp_t *dp, uint8_t id, const char *str)
{
    dp->id = id;
    dp->type = DP_TYPE_STRING;
    dp->len = strlen(str);
    if (dp->len >= sizeof(dp->data.str))
        dp->len = sizeof(dp->data.str) - 1;
    memcpy(dp->data.str, str, dp->len);
    dp->data.str[dp->len] = '\0'; // Null-terminate for local use
}

void tuya_dp_set_enum(tuya_dp_t *dp, uint8_t id, uint8_t value)
{
    dp->id = id;
    dp->type = DP_TYPE_ENUM;
    dp->len = 1;
    dp->data.raw[0] = value;
}

void tuya_dp_set_bitmap(tuya_dp_t *dp, uint8_t id, const uint8_t *bits, uint16_t len)
{
    dp->id = id;
    dp->type = DP_TYPE_BITMAP;
    dp->len = (len <= sizeof(dp->data.bitmap)) ? len : sizeof(dp->data.bitmap);
    memcpy(dp->data.bitmap, bits, dp->len);
}

uint16_t tuya_dp_get_len(const tuya_dp_t *dp)
{
    if (!dp)
        return 0;

    switch (dp->type) {
    case DP_TYPE_BOOL:
        return 4 + 1;
    case DP_TYPE_VALUE:
        return 4 + 4;
    case DP_TYPE_STRING:
        return 4 + strlen(dp->data.str);
    case DP_TYPE_ENUM:
        return 4 + 1;
    case DP_TYPE_RAW:
        return 4 + dp->len;
    case DP_TYPE_BITMAP:
        return 4 + dp->len;
    default:
        return 4 + dp->len;
    }
}

int tuya_dp_serialize(const tuya_dp_t *dp, uint8_t *out_buf, size_t out_len)
{
    if (!dp || !out_buf)
        return -1;

    uint16_t payload_len = 0;

    switch (dp->type) {
    case DP_TYPE_BOOL:
        payload_len = 1;
        break;
    case DP_TYPE_VALUE:
        payload_len = 4;
        break;
    case DP_TYPE_STRING:
        payload_len = strlen(dp->data.str);
        break;
    case DP_TYPE_ENUM:
        payload_len = 1;
        break;
    case DP_TYPE_RAW:
        payload_len = dp->len;
        break;
    case DP_TYPE_BITMAP:
        payload_len = dp->len;
        break;
    default:
        payload_len = dp->len;
        break;
    }

    size_t total_len = 4 + payload_len;
    if (out_len < total_len)
        return -1;

    // Header
    out_buf[0] = dp->id;
    out_buf[1] = dp->type;
    out_buf[2] = (payload_len >> 8) & 0xFF; // LEN_H (big-endian)
    out_buf[3] = payload_len & 0xFF;        // LEN_L

    // Value
    memcpy(&out_buf[4], dp->data.raw, payload_len);

    return (int)total_len;
}

int parse_tuya_dp(const uint8_t *buf, size_t buf_len, tuya_dp_t *dp)
{
    if (!buf || !dp || buf_len < 4) {
        return -1; // Invalid input
    }

    dp->id = buf[0];
    dp->type = buf[1];
    dp->len = (uint16_t)((buf[2] << 8) | buf[3]); // Big endian

    if (dp->len > sizeof(dp->data.raw)) {
        return -1; // Too large for struct
    }

    if (buf_len < 4 + dp->len) {
        return -1; // Not enough data
    }

    // Copy raw bytes
    memcpy(dp->data.raw, buf + 4, dp->len);

    // Interpret according to DP type
    switch (dp->type) {
    case DP_TYPE_BOOL:
        dp->data.boolean = (dp->data.raw[0] != 0);
        break;

    case DP_TYPE_VALUE:
        // Tuya values are 4-byte big-endian integers
        if (dp->len >= 4) {
            dp->data.value =
                (int32_t)(((uint32_t)dp->data.raw[0] << 24) | ((uint32_t)dp->data.raw[1] << 16) |
                          ((uint32_t)dp->data.raw[2] << 8) | ((uint32_t)dp->data.raw[3]));
        }
        break;

    case DP_TYPE_STRING:
        // Ensure null-terminated string
        if (dp->len < sizeof(dp->data.str)) {
            dp->data.str[dp->len] = '\0';
        } else {
            dp->data.str[sizeof(dp->data.str) - 1] = '\0';
        }
        break;

    case DP_TYPE_ENUM:
        // Enum is usually 1 or 2 bytes — just store raw
        break;

    case DP_TYPE_BITMAP:
        // Bitmap is usually 1–4 bytes — already in raw
        break;

    case DP_TYPE_RAW:
    default:
        // Leave as raw data
        break;
    }
    return 0;
}

int tuya_dp_print(tuya_dp_t *dp)
{
    static const char *type_str[] = {
        [DP_TYPE_RAW] = "RAW",       [DP_TYPE_BOOL] = "BOOL", [DP_TYPE_VALUE] = "VALUE",
        [DP_TYPE_STRING] = "STRING", [DP_TYPE_ENUM] = "ENUM", [DP_TYPE_BITMAP] = "BITMAP"
    };

    if (!dp) {
        return -1;
    }

    const char *tstr = "UNKNOWN";
    if (dp->type <= DP_TYPE_BITMAP && type_str[dp->type])
        tstr = type_str[dp->type];

    printf("DP id: %d, type: %s[%d], len: %d ", dp->id, tstr, dp->type, dp->len);
    switch (dp->type) {
    case DP_TYPE_RAW:
        printf("Raw Data: ");
        for (int i = 0; i < dp->len && i < (int)sizeof(dp->data.raw); ++i)
            printf("%02X ", dp->data.raw[i]);
        printf("\n");
        break;

    case DP_TYPE_BOOL:
        printf("Boolean Value: %s\n", dp->data.boolean ? "true" : "false");
        break;

    case DP_TYPE_VALUE:
        printf("Integer Value: %" PRIu32 "\n", dp->data.value);
        break;

    case DP_TYPE_STRING:
        printf("String Value: %.*s\n", dp->len, dp->data.str);
        break;

    case DP_TYPE_ENUM:
        if (dp->len > 0)
            printf("Enum Value: %u\n", dp->data.raw[0]);
        else
            printf("Enum Value: (empty)\n");
        break;

    case DP_TYPE_BITMAP:
        printf("Bitmap Value: ");
        for (int i = 0; i < dp->len && i < (int)sizeof(dp->data.bitmap); ++i)
            printf("%02X ", dp->data.bitmap[i]);
        printf("\n");
        break;

    default:
        printf("Unknown Data Point Type\n");
        break;
    }

    return 0; // Success
}
