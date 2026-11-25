/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "thermostat-controller.h"
#include <math.h>
#include <string.h>

#define THERMOSTAT_CONTROLLER_INVALID_ARGUMENT (-1)
#define THERMOSTAT_CONTROLLER_INVALID_TEMPERATURE (-2)

static int set_heating(struct thermostat_controller *controller, bool enabled)
{
    int rc;

    if (controller->heating_state_known && controller->heating == enabled)
        return 0;

    rc = controller->hal.set_heating(controller->hal.context, enabled);
    if (rc == 0) {
        controller->heating = enabled;
        controller->heating_state_known = true;
    }
    return rc;
}

int thermostat_controller_init(struct thermostat_controller              *controller,
                               const struct thermostat_hal               *hal,
                               const struct thermostat_controller_config *config)
{
    if (!controller || !hal || !config || !hal->read_temperature_c || !hal->set_heating)
        return THERMOSTAT_CONTROLLER_INVALID_ARGUMENT;

    memset(controller, 0, sizeof(*controller));
    controller->hal = *hal;
    controller->setpoint_c = config->setpoint_c;
    controller->hysteresis_c = config->hysteresis_c;

    if (thermostat_controller_set_setpoint(controller, config->setpoint_c) != 0 ||
        thermostat_controller_set_hysteresis(controller, config->hysteresis_c) != 0)
        return THERMOSTAT_CONTROLLER_INVALID_ARGUMENT;

    return set_heating(controller, false);
}

int thermostat_controller_set_setpoint(struct thermostat_controller *controller, float setpoint_c)
{
    if (!controller || !isfinite(setpoint_c) || setpoint_c < THERMOSTAT_CONTROLLER_MIN_SETPOINT_C ||
        setpoint_c > THERMOSTAT_CONTROLLER_MAX_SETPOINT_C)
        return THERMOSTAT_CONTROLLER_INVALID_ARGUMENT;

    controller->setpoint_c = setpoint_c;
    return 0;
}

int thermostat_controller_set_hysteresis(struct thermostat_controller *controller,
                                         float                         hysteresis_c)
{
    if (!controller || !isfinite(hysteresis_c) || hysteresis_c <= 0.0f || hysteresis_c > 20.0f)
        return THERMOSTAT_CONTROLLER_INVALID_ARGUMENT;

    controller->hysteresis_c = hysteresis_c;
    return 0;
}

void thermostat_controller_set_enabled(struct thermostat_controller *controller, bool enabled)
{
    if (controller)
        controller->enabled = enabled;
}

int thermostat_controller_update(struct thermostat_controller *controller)
{
    float temperature_c;
    bool  target_heating;
    int   rc;

    if (!controller)
        return THERMOSTAT_CONTROLLER_INVALID_ARGUMENT;

    if (!controller->enabled)
        return set_heating(controller, false);

    rc = controller->hal.read_temperature_c(controller->hal.context, &temperature_c);
    if (rc != 0 || !isfinite(temperature_c)) {
        int off_rc = set_heating(controller, false);
        if (off_rc != 0)
            return off_rc;
        return rc != 0 ? rc : THERMOSTAT_CONTROLLER_INVALID_TEMPERATURE;
    }

    if (controller->heating_state_known && controller->heating) {
        target_heating = temperature_c < controller->setpoint_c + controller->hysteresis_c / 2.0f;
    } else {
        target_heating = temperature_c <= controller->setpoint_c - controller->hysteresis_c / 2.0f;
    }

    return set_heating(controller, target_heating);
}

bool thermostat_controller_is_heating(const struct thermostat_controller *controller)
{
    return controller && controller->heating_state_known && controller->heating;
}
