/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_THERMOSTAT_CHANNEL_H_
#define SUPLA_THERMOSTAT_CHANNEL_H_

#include <stdbool.h>
#include <stdint.h>
#include <libsupla/channel.h>

/**
 * Hardware callbacks used by the thermostat controller. Return 0 on success.
 */
struct thermostat_channel_hal {
    void *user_ctx;
    int (*read_temperature_c)(void *ctx, float *temperature_c);
    int (*set_heating)(void *ctx, bool enabled);
};

/**
 * Configuration for a heating-only SUPLA HVAC channel.
 *
 * The thermometer channel must be added to the same SUPLA device before this
 * channel so that its channel number can be sent in the HVAC configuration.
 * Weekly schedules use the device's local time and timezone; configure its
 * clock before enabling a schedule in the SUPLA app. A failed temperature read
 * forces the relay off.
 */
struct thermostat_channel_config {
    struct thermostat_channel_hal hal;
    supla_channel_t              *thermometer_channel;
    float                         setpoint_c;
    float    hysteresis_c;     /**< Degrees Celsius; 0 selects the controller default. */
    uint32_t poll_interval_ms; /**< Milliseconds; 0 selects a 10-second interval. */
    bool     initially_enabled;
};

supla_channel_t *supla_thermostat_channel_create(const struct thermostat_channel_config *config);
int              supla_thermostat_channel_delete(supla_channel_t *channel);
int              supla_thermostat_channel_set_setpoint(supla_channel_t *channel, float setpoint_c);
int              supla_thermostat_channel_set_enabled(supla_channel_t *channel, bool enabled);

#endif /* SUPLA_THERMOSTAT_CHANNEL_H_ */
