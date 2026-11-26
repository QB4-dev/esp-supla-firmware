/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef SUPLA_HVAC_CONTROLLER_H_
#define SUPLA_HVAC_CONTROLLER_H_

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Channel adapters resolve manual or scheduled settings into controller modes. */
#define HVAC_CONTROLLER_DEFAULT_HYSTERESIS_C 0.5f
#define HVAC_CONTROLLER_MIN_SETPOINT_C (-50.0f)
#define HVAC_CONTROLLER_MAX_SETPOINT_C 100.0f

enum hvac_controller_mode {
    HVAC_CONTROLLER_MODE_OFF,
    HVAC_CONTROLLER_MODE_HEAT,
    HVAC_CONTROLLER_MODE_COOL,
    HVAC_CONTROLLER_MODE_HEAT_COOL,
};

enum hvac_controller_output {
    HVAC_CONTROLLER_OUTPUT_OFF,
    HVAC_CONTROLLER_OUTPUT_HEATING,
    HVAC_CONTROLLER_OUTPUT_COOLING,
};

/**
 * The output callback must select exactly one output, or turn all outputs off.
 */
struct hvac_controller_hal {
    void *context;
    int (*read_temperature_c)(void *context, float *temperature_c);
    int (*set_output)(void *context, enum hvac_controller_output output);
};

struct hvac_controller_config {
    enum hvac_controller_mode mode;
    float heating_setpoint_c;
    float cooling_setpoint_c;
    float hysteresis_c;
};

struct hvac_controller {
    struct hvac_controller_hal hal;
    enum hvac_controller_mode  mode;
    enum hvac_controller_output output;
    float                      heating_setpoint_c;
    float                      cooling_setpoint_c;
    float                      hysteresis_c;
    bool                       output_state_known;
};

int  hvac_controller_init(struct hvac_controller *controller, const struct hvac_controller_hal *hal,
                          const struct hvac_controller_config *config);
int  hvac_controller_set_mode(struct hvac_controller *controller, enum hvac_controller_mode mode);
int  hvac_controller_set_setpoints(struct hvac_controller *controller, float heating_setpoint_c,
                                   float cooling_setpoint_c);
int  hvac_controller_set_heating_setpoint(struct hvac_controller *controller, float setpoint_c);
int  hvac_controller_set_cooling_setpoint(struct hvac_controller *controller, float setpoint_c);
int  hvac_controller_set_hysteresis(struct hvac_controller *controller, float hysteresis_c);
int  hvac_controller_update(struct hvac_controller *controller);
int  hvac_controller_update_with_temperature(struct hvac_controller *controller, float temperature_c);
bool hvac_controller_is_heating(const struct hvac_controller *controller);
bool hvac_controller_is_cooling(const struct hvac_controller *controller);

#ifdef __cplusplus
}
#endif

#endif /* SUPLA_HVAC_CONTROLLER_H_ */
