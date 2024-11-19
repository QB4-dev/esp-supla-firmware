/*
 * settings-def.h
 *
 *  Created on: 15 lut 2024
 *      Author: kuba
 */

#ifndef SETTINGS_DEF_H_
#define SETTINGS_DEF_H_

#include <stdbool.h>
#include <inttypes.h>

typedef enum {
    SETTING_TYPE_BOOL = 0,
    SETTING_TYPE_NUM = 1,
    SETTING_TYPE_ONEOF = 2,
    SETTING_TYPE_TIME = 3,
    SETTING_TYPE_COLOR = 4,
} setting_type_t;

typedef struct {
    bool val;
    bool def;
} setting_bool_t;

typedef struct {
    int val;
    int def;
    int range[2];
} setting_int_t;

typedef struct {
    int          val;
    int          def;
    const char **options;
} setting_oneof_t;

typedef struct {
    int hh;
    int mm;
} setting_time_t;

typedef union {
    struct {
        unsigned char b;
        unsigned char g;
        unsigned char r;
        unsigned char w;
    };
    uint32_t combined;
} setting_color_t;

typedef struct {
    const char    *id;    //short ID
    const char    *label; //more descriptive
    setting_type_t type;
    bool           disabled;
    union {
        setting_bool_t  boolean;
        setting_int_t   num;
        setting_oneof_t oneof;
        setting_time_t  time;
        setting_color_t color;
    };
} setting_t;

typedef struct {
    const char *id;    //short ID
    const char *label; //more descriptive
    setting_t  *settings;
} settings_group_t;

#endif /* SETTINGS_DEF_H_ */
