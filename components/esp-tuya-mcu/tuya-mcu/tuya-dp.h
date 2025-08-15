#pragma once

#include <stdbool.h>
#include <inttypes.h>
#include <stddef.h>

#include "tuya-defs.h"

typedef struct {
    uint8_t  id;   // Data point ID
    uint8_t  type; // Data point type
    uint16_t len;  // Length of data point value
    union {
        uint8_t raw[64];   // Raw data (max 16 bytes)
        bool    boolean;   // Boolean value
        int32_t value;     // Integer value
        char    str[64];   // String value
        uint8_t bitmap[4]; // Bitmap value
    } data;                // Data point value
} tuya_dp_t;

void     tuya_dp_set_raw(tuya_dp_t *dp, uint8_t id, const uint8_t *buf, uint16_t len);
void     tuya_dp_set_bool(tuya_dp_t *dp, uint8_t id, bool value);
void     tuya_dp_set_value(tuya_dp_t *dp, uint8_t id, int32_t value);
void     tuya_dp_set_string(tuya_dp_t *dp, uint8_t id, const char *str);
void     tuya_dp_set_enum(tuya_dp_t *dp, uint8_t id, uint8_t value);
void     tuya_dp_set_bitmap(tuya_dp_t *dp, uint8_t id, const uint8_t *bits, uint16_t len);
uint16_t tuya_dp_get_len(const tuya_dp_t *dp);

int parse_tuya_dp(const uint8_t *data, size_t len, tuya_dp_t *dp);
int tuya_dp_print(tuya_dp_t *dp);
