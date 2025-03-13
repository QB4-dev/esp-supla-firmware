/*
 * Copyright (c) 2025 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SETTINGS_DEF_H_
#define SETTINGS_DEF_H_

#include <stdbool.h>
#include <inttypes.h>

typedef enum {
    SETTING_TYPE_BOOL = 0,
    SETTING_TYPE_NUM,
    SETTING_TYPE_ONEOF,
    SETTING_TYPE_TEXT,
#ifdef CONFIG_SETTINGS_DATETIME_SUPPORT
    SETTING_TYPE_TIME,
    SETTING_TYPE_DATE,
    SETTING_TYPE_DATETIME,
#endif

#ifdef CONFIG_SETTINGS_TIMEZONE_SUPPORT
    SETTING_TYPE_TIMEZONE,
#endif

#ifdef CONFIG_SETTINGS_COLOR_SUPPORT
    SETTING_TYPE_COLOR,
#endif
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
    char       *val;
    const char *def;
    size_t      len;
} setting_text_t;

#ifdef CONFIG_SETTINGS_DATETIME_SUPPORT
typedef struct {
    int hh;
    int mm;
} setting_time_t;

typedef struct {
    int day;
    int month;
    int year;
} setting_date_t;

typedef struct {
    setting_time_t time;
    setting_date_t date;
} setting_datetime_t;
#endif

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
        setting_text_t  text;
#ifdef CONFIG_SETTINGS_DATETIME_SUPPORT
        setting_time_t     time;
        setting_date_t     date;
        setting_datetime_t datetime;
#endif
        setting_text_t  timezone;
        setting_color_t color;
    };
} setting_t;

typedef struct {
    const char *id;    //short ID
    const char *label; //more descriptive
    setting_t  *settings;
} settings_group_t;

#endif /* SETTINGS_DEF_H_ */
