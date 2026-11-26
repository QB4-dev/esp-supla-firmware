/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "hvac-controller.h"
#include <math.h>
#include <string.h>

#define HVAC_CONTROLLER_INVALID_ARGUMENT (-1)
#define HVAC_CONTROLLER_INVALID_TEMPERATURE (-2)

static bool valid_mode(enum hvac_controller_mode mode)
{
    return mode == HVAC_CONTROLLER_MODE_OFF || mode == HVAC_CONTROLLER_MODE_HEAT ||
           mode == HVAC_CONTROLLER_MODE_COOL || mode == HVAC_CONTROLLER_MODE_HEAT_COOL;
}

static bool valid_setpoint(float setpoint_c)
{
    return isfinite(setpoint_c) && setpoint_c >= HVAC_CONTROLLER_MIN_SETPOINT_C &&
           setpoint_c <= HVAC_CONTROLLER_MAX_SETPOINT_C;
}

static int set_output(struct hvac_controller *controller, enum hvac_controller_output output)
{
    int rc;

    if (controller->output_state_known && controller->output == output)
        return 0;

    if (controller->output_state_known && controller->output != HVAC_CONTROLLER_OUTPUT_OFF &&
        output != HVAC_CONTROLLER_OUTPUT_OFF) {
        rc = controller->hal.set_output(controller->hal.context, HVAC_CONTROLLER_OUTPUT_OFF);
        if (rc != 0)
            return rc;
        controller->output = HVAC_CONTROLLER_OUTPUT_OFF;
    }

    rc = controller->hal.set_output(controller->hal.context, output);
    if (rc == 0) {
        controller->output = output;
        controller->output_state_known = true;
    }
    return rc;
}

int hvac_controller_init(struct hvac_controller *controller, const struct hvac_controller_hal *hal,
                         const struct hvac_controller_config *config)
{
    if (!controller || !hal || !config || !hal->read_temperature_c || !hal->set_output ||
        !valid_mode(config->mode) || !valid_setpoint(config->heating_setpoint_c) ||
        !valid_setpoint(config->cooling_setpoint_c) || !isfinite(config->hysteresis_c) ||
        config->hysteresis_c <= 0.0f || config->hysteresis_c > 20.0f ||
        (config->mode == HVAC_CONTROLLER_MODE_HEAT_COOL &&
         config->heating_setpoint_c >= config->cooling_setpoint_c))
        return HVAC_CONTROLLER_INVALID_ARGUMENT;

    memset(controller, 0, sizeof(*controller));
    controller->hal = *hal;
    controller->mode = config->mode;
    controller->heating_setpoint_c = config->heating_setpoint_c;
    controller->cooling_setpoint_c = config->cooling_setpoint_c;
    controller->hysteresis_c = config->hysteresis_c;

    return set_output(controller, HVAC_CONTROLLER_OUTPUT_OFF);
}

int hvac_controller_set_mode(struct hvac_controller *controller, enum hvac_controller_mode mode)
{
    if (!controller || !valid_mode(mode) ||
        (mode == HVAC_CONTROLLER_MODE_HEAT_COOL &&
         controller->heating_setpoint_c >= controller->cooling_setpoint_c))
        return HVAC_CONTROLLER_INVALID_ARGUMENT;

    controller->mode = mode;
    return 0;
}

int hvac_controller_set_setpoints(struct hvac_controller *controller, float heating_setpoint_c,
                                  float cooling_setpoint_c)
{
    if (!controller || !valid_setpoint(heating_setpoint_c) || !valid_setpoint(cooling_setpoint_c) ||
        (controller->mode == HVAC_CONTROLLER_MODE_HEAT_COOL &&
         heating_setpoint_c >= cooling_setpoint_c))
        return HVAC_CONTROLLER_INVALID_ARGUMENT;

    controller->heating_setpoint_c = heating_setpoint_c;
    controller->cooling_setpoint_c = cooling_setpoint_c;
    return 0;
}

int hvac_controller_set_heating_setpoint(struct hvac_controller *controller, float setpoint_c)
{
    if (!controller)
        return HVAC_CONTROLLER_INVALID_ARGUMENT;
    return hvac_controller_set_setpoints(controller, setpoint_c, controller->cooling_setpoint_c);
}

int hvac_controller_set_cooling_setpoint(struct hvac_controller *controller, float setpoint_c)
{
    if (!controller)
        return HVAC_CONTROLLER_INVALID_ARGUMENT;
    return hvac_controller_set_setpoints(controller, controller->heating_setpoint_c, setpoint_c);
}

int hvac_controller_set_hysteresis(struct hvac_controller *controller, float hysteresis_c)
{
    if (!controller || !isfinite(hysteresis_c) || hysteresis_c <= 0.0f || hysteresis_c > 20.0f)
        return HVAC_CONTROLLER_INVALID_ARGUMENT;
    controller->hysteresis_c = hysteresis_c;
    return 0;
}

int hvac_controller_update_with_temperature(struct hvac_controller *controller, float temperature_c)
{
    float                        half_hysteresis;
    enum hvac_controller_output target_output = HVAC_CONTROLLER_OUTPUT_OFF;

    if (!controller)
        return HVAC_CONTROLLER_INVALID_ARGUMENT;

    if (controller->mode == HVAC_CONTROLLER_MODE_OFF)
        return set_output(controller, HVAC_CONTROLLER_OUTPUT_OFF);

    if (!isfinite(temperature_c)) {
        int off_rc = set_output(controller, HVAC_CONTROLLER_OUTPUT_OFF);
        if (off_rc != 0)
            return off_rc;
        return HVAC_CONTROLLER_INVALID_TEMPERATURE;
    }

    half_hysteresis = controller->hysteresis_c / 2.0f;
    switch (controller->mode) {
    case HVAC_CONTROLLER_MODE_HEAT:
        target_output =
            (controller->output == HVAC_CONTROLLER_OUTPUT_HEATING ?
                 temperature_c < controller->heating_setpoint_c + half_hysteresis :
                 temperature_c <= controller->heating_setpoint_c - half_hysteresis) ?
                HVAC_CONTROLLER_OUTPUT_HEATING :
                HVAC_CONTROLLER_OUTPUT_OFF;
        break;
    case HVAC_CONTROLLER_MODE_COOL:
        target_output =
            (controller->output == HVAC_CONTROLLER_OUTPUT_COOLING ?
                 temperature_c > controller->cooling_setpoint_c - half_hysteresis :
                 temperature_c >= controller->cooling_setpoint_c + half_hysteresis) ?
                HVAC_CONTROLLER_OUTPUT_COOLING :
                HVAC_CONTROLLER_OUTPUT_OFF;
        break;
    case HVAC_CONTROLLER_MODE_HEAT_COOL:
        if (controller->output == HVAC_CONTROLLER_OUTPUT_HEATING) {
            if (temperature_c >= controller->cooling_setpoint_c + half_hysteresis)
                target_output = HVAC_CONTROLLER_OUTPUT_COOLING;
            else if (temperature_c < controller->heating_setpoint_c + half_hysteresis)
                target_output = HVAC_CONTROLLER_OUTPUT_HEATING;
        } else if (controller->output == HVAC_CONTROLLER_OUTPUT_COOLING) {
            if (temperature_c <= controller->heating_setpoint_c - half_hysteresis)
                target_output = HVAC_CONTROLLER_OUTPUT_HEATING;
            else if (temperature_c > controller->cooling_setpoint_c - half_hysteresis)
                target_output = HVAC_CONTROLLER_OUTPUT_COOLING;
        } else if (temperature_c <= controller->heating_setpoint_c - half_hysteresis) {
            target_output = HVAC_CONTROLLER_OUTPUT_HEATING;
        } else if (temperature_c >= controller->cooling_setpoint_c + half_hysteresis) {
            target_output = HVAC_CONTROLLER_OUTPUT_COOLING;
        }
        break;
    case HVAC_CONTROLLER_MODE_OFF:
        break;
    }

    return set_output(controller, target_output);
}

int hvac_controller_update(struct hvac_controller *controller)
{
    float temperature_c;
    int   rc;

    if (!controller)
        return HVAC_CONTROLLER_INVALID_ARGUMENT;
    if (controller->mode == HVAC_CONTROLLER_MODE_OFF)
        return set_output(controller, HVAC_CONTROLLER_OUTPUT_OFF);

    rc = controller->hal.read_temperature_c(controller->hal.context, &temperature_c);
    if (rc != 0) {
        int off_rc = set_output(controller, HVAC_CONTROLLER_OUTPUT_OFF);
        return off_rc != 0 ? off_rc : rc;
    }

    return hvac_controller_update_with_temperature(controller, temperature_c);
}

bool hvac_controller_is_heating(const struct hvac_controller *controller)
{
    return controller && controller->output_state_known &&
           controller->output == HVAC_CONTROLLER_OUTPUT_HEATING;
}

bool hvac_controller_is_cooling(const struct hvac_controller *controller)
{
    return controller && controller->output_state_known &&
           controller->output == HVAC_CONTROLLER_OUTPUT_COOLING;
}
