/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_THERMOSTAT_CONTROLLER_H_
#define SUPLA_THERMOSTAT_CONTROLLER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define THERMOSTAT_CONTROLLER_DEFAULT_HYSTERESIS_C 0.5f
#define THERMOSTAT_CONTROLLER_MIN_SETPOINT_C (-50.0f)
#define THERMOSTAT_CONTROLLER_MAX_SETPOINT_C 100.0f

struct thermostat_hal {
    void *context;
    int (*read_temperature_c)(void *context, float *temperature_c);
    int (*set_heating)(void *context, bool enabled);
};

struct thermostat_controller_config {
    float setpoint_c;
    float hysteresis_c;
};

struct thermostat_controller {
    struct thermostat_hal hal;
    float                 setpoint_c;
    float                 hysteresis_c;
    bool                  enabled;
    bool                  heating;
    bool                  heating_state_known;
};

int  thermostat_controller_init(struct thermostat_controller              *controller,
                                const struct thermostat_hal               *hal,
                                const struct thermostat_controller_config *config);
int  thermostat_controller_set_setpoint(struct thermostat_controller *controller, float setpoint_c);
int  thermostat_controller_set_hysteresis(struct thermostat_controller *controller,
                                          float                         hysteresis_c);
void thermostat_controller_set_enabled(struct thermostat_controller *controller, bool enabled);
int  thermostat_controller_update(struct thermostat_controller *controller);
bool thermostat_controller_is_heating(const struct thermostat_controller *controller);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_THERMOSTAT_CONTROLLER_H_ */
