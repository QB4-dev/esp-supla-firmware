/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/thermostat-channel.h"
#include "thermostat-controller.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "THERMOSTAT";
#define THERMOSTAT_DEFAULT_POLL_INTERVAL_MS 10000
#define THERMOSTAT_MIN_VALID_EPOCH 1577836800

struct thermostat_channel_data {
    struct thermostat_controller  controller;
    supla_channel_t              *thermometer_channel;
    esp_timer_handle_t            timer;
    SemaphoreHandle_t             mutex;
    uint32_t                      poll_interval_ms;
    TChannelConfig_WeeklySchedule schedule;
    bool                          schedule_valid;
    bool                          weekly_schedule;
    bool                          manual_enabled;
    bool                          sensor_error;
    bool                          clock_error;
    float                         manual_setpoint_c;
};

static bool is_thermometer_channel(supla_channel_t *channel)
{
    supla_channel_config_t config;

    if (!channel || supla_channel_get_config(channel, &config) != SUPLA_RESULT_TRUE)
        return false;

    switch (config.type) {
    case SUPLA_CHANNELTYPE_THERMOMETER:
    case SUPLA_CHANNELTYPE_THERMOMETERDS18B20:
    case SUPLA_CHANNELTYPE_DHT11:
    case SUPLA_CHANNELTYPE_DHT21:
    case SUPLA_CHANNELTYPE_DHT22:
    case SUPLA_CHANNELTYPE_AM2301:
    case SUPLA_CHANNELTYPE_AM2302:
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
        return true;
    default:
        return false;
    }
}

static int16_t temperature_to_protocol(float temperature_c)
{
    float scaled = temperature_c * 100.0f;
    return (int16_t)(scaled + (scaled < 0.0f ? -0.5f : 0.5f));
}

static int schedule_program_id(const TChannelConfig_WeeklySchedule *schedule, int weekday, int hour,
                               int minute)
{
    unsigned int quarter =
        (unsigned int)weekday * 96 + (unsigned int)hour * 4 + (unsigned int)minute / 15;
    unsigned char value = schedule->Quarters[quarter / 2];

    return quarter % 2 ? (value >> 4) & 0x0f : value & 0x0f;
}

static int validate_schedule(const TChannelConfig_WeeklySchedule *schedule)
{
    bool         used[SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE] = {};
    unsigned int quarter;

    for (quarter = 0; quarter < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; ++quarter) {
        unsigned char packed = schedule->Quarters[quarter / 2];
        unsigned int  program_id = quarter % 2 ? (packed >> 4) & 0x0f : packed & 0x0f;
        if (program_id > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE)
            return ESP_ERR_INVALID_ARG;
        if (program_id)
            used[program_id - 1] = true;
    }

    for (quarter = 0; quarter < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; ++quarter) {
        if (!used[quarter])
            continue;
        const TWeeklyScheduleProgram *program = &schedule->Program[quarter];
        if (program->Mode != SUPLA_HVAC_MODE_OFF && program->Mode != SUPLA_HVAC_MODE_HEAT)
            return ESP_ERR_INVALID_ARG;
        if (program->Mode == SUPLA_HVAC_MODE_HEAT) {
            float setpoint = program->SetpointTemperatureHeat / 100.0f;
            if (!isfinite(setpoint) || setpoint < THERMOSTAT_CONTROLLER_MIN_SETPOINT_C ||
                setpoint > THERMOSTAT_CONTROLLER_MAX_SETPOINT_C)
                return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

static bool get_local_time(struct tm *local_time)
{
    time_t now = time(NULL);
    return now >= (time_t)THERMOSTAT_MIN_VALID_EPOCH && localtime_r(&now, local_time) != NULL;
}

static int update_channel_value(supla_channel_t *channel, struct thermostat_channel_data *data)
{
    struct tm  local_time;
    THVACValue value = {};
    float      target_setpoint = data->manual_setpoint_c;
    bool       target_enabled = data->manual_enabled && !data->weekly_schedule;
    int        rc;

    data->clock_error = false;
    if (data->weekly_schedule) {
        if (!data->schedule_valid || !get_local_time(&local_time)) {
            data->clock_error = true;
            target_enabled = false;
        } else {
            int program_id = schedule_program_id(&data->schedule, local_time.tm_wday,
                                                 local_time.tm_hour, local_time.tm_min);
            if (program_id > 0) {
                const TWeeklyScheduleProgram *program = &data->schedule.Program[program_id - 1];
                target_enabled = program->Mode == SUPLA_HVAC_MODE_HEAT;
                if (target_enabled)
                    target_setpoint = program->SetpointTemperatureHeat / 100.0f;
            } else {
                target_enabled = false;
            }
        }
    }

    rc = thermostat_controller_set_setpoint(&data->controller, target_setpoint);
    if (rc != 0)
        return ESP_ERR_INVALID_ARG;

    thermostat_controller_set_enabled(&data->controller, target_enabled);
    rc = thermostat_controller_update(&data->controller);
    data->sensor_error = rc != 0;
    if (rc != 0)
        ESP_LOGE(TAG, "Thermostat update failed: %d", rc);

    value.IsOn = (data->weekly_schedule || data->manual_enabled) ? 1 : 0;
    value.Mode = value.IsOn ? SUPLA_HVAC_MODE_HEAT : SUPLA_HVAC_MODE_OFF;
    value.SetpointTemperatureHeat = temperature_to_protocol(target_setpoint);
    value.Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    if (data->weekly_schedule)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
    if (thermostat_controller_is_heating(&data->controller))
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_HEATING;
    if (data->sensor_error)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
    if (data->clock_error)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;

    if (supla_channel_set_hvac_value(channel, &value) != SUPLA_RESULT_TRUE)
        return ESP_FAIL;
    return rc == 0 ? ESP_OK : rc;
}

static void thermostat_timer(void *arg)
{
    supla_channel_t                *channel = arg;
    struct thermostat_channel_data *data = supla_channel_get_data(channel);

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) == pdTRUE) {
        update_channel_value(channel, data);
        xSemaphoreGive(data->mutex);
    }
}

static int thermostat_channel_init(supla_channel_t *channel)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    esp_err_t                       rc;

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);

    rc = esp_timer_start_periodic(data->timer, (uint64_t)data->poll_interval_ms * 1000);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start thermostat timer: %s", esp_err_to_name(rc));
        return rc;
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int thermostat_set_value(supla_channel_t *channel, TSD_SuplaChannelNewValue *new_value)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    const THVACValue               *value = (const THVACValue *)new_value->value;
    float                           setpoint;
    int                             rc = SUPLA_RESULTCODE_TRUE;

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    switch (value->Mode) {
    case SUPLA_HVAC_MODE_NOT_SET:
    case SUPLA_HVAC_MODE_OFF:
    case SUPLA_HVAC_MODE_HEAT:
    case SUPLA_HVAC_MODE_CMD_TURN_ON:
    case SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE:
    case SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL:
        break;
    default:
        xSemaphoreGive(data->mutex);
        ESP_LOGE(TAG, "Unsupported HVAC mode: %u", value->Mode);
        return ESP_ERR_INVALID_ARG;
    }

    if (value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET) {
        setpoint = value->SetpointTemperatureHeat / 100.0f;
        rc = thermostat_controller_set_setpoint(&data->controller, setpoint);
        if (rc != 0) {
            xSemaphoreGive(data->mutex);
            ESP_LOGE(TAG, "Rejected invalid setpoint: %.2f C", (double)setpoint);
            return ESP_ERR_INVALID_ARG;
        }
        data->manual_setpoint_c = setpoint;
    }

    switch (value->Mode) {
    case SUPLA_HVAC_MODE_NOT_SET:
        if (value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET) {
            data->weekly_schedule = false;
            data->manual_enabled = true;
        }
        break;
    case SUPLA_HVAC_MODE_OFF:
        data->weekly_schedule = false;
        data->manual_enabled = false;
        break;
    case SUPLA_HVAC_MODE_HEAT:
        data->weekly_schedule = false;
        data->manual_enabled = true;
        break;
    case SUPLA_HVAC_MODE_CMD_TURN_ON:
        data->weekly_schedule = false;
        data->manual_enabled = true;
        break;
    case SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE:
        data->weekly_schedule = true;
        break;
    case SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL:
        data->weekly_schedule = false;
        data->manual_enabled = true;
        break;
    default:
        break;
    }

    rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);
    return rc == ESP_OK ? SUPLA_RESULTCODE_TRUE : rc;
}

static int thermostat_config_set(supla_channel_t *channel, TSDS_SetChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    supla_channel_config_t          thermometer_config;
    int thermometer_number = supla_channel_get_assigned_number(data->thermometer_channel);
    TChannelConfig_HVAC hvac_config = {};
    int                 rc;

    if (thermometer_number < 0 ||
        supla_channel_get_config(data->thermometer_channel, &thermometer_config) !=
            SUPLA_RESULT_TRUE ||
        !is_thermometer_channel(data->thermometer_channel)) {
        ESP_LOGE(TAG, "Thermometer channel must be added before the thermostat");
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    hvac_config.MainThermometerChannelNo = thermometer_number;
    hvac_config.AuxThermometerType = SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET;
    hvac_config.AvailableAlgorithms = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
    hvac_config.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
    hvac_config.Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
    hvac_config.TemperatureSetpointChangeSwitchesToManualMode = 1;
    hvac_config.TemperatureControlType = SUPLA_HVAC_TEMPERATURE_CONTROL_TYPE_ROOM_TEMPERATURE;
    hvac_config.ParameterFlags.MainThermometerChannelNoReadonly = 1;
    hvac_config.ParameterFlags.TemperaturesRoomMinReadonly = 1;
    hvac_config.ParameterFlags.TemperaturesRoomMaxReadonly = 1;
    hvac_config.Temperatures.Index = TEMPERATURE_HISTERESIS | TEMPERATURE_ROOM_MIN |
                                     TEMPERATURE_ROOM_MAX;
    hvac_config.Temperatures.Temperature[TEMPERATURE_HISTERESIS] =
        temperature_to_protocol(data->controller.hysteresis_c);
    hvac_config.Temperatures.Temperature[TEMPERATURE_ROOM_MIN] =
        temperature_to_protocol(THERMOSTAT_CONTROLLER_MIN_SETPOINT_C);
    hvac_config.Temperatures.Temperature[TEMPERATURE_ROOM_MAX] =
        temperature_to_protocol(THERMOSTAT_CONTROLLER_MAX_SETPOINT_C);
    xSemaphoreGive(data->mutex);

    config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    config->ConfigSize = sizeof(hvac_config);
    memcpy(config->Config, &hvac_config, sizeof(hvac_config));
    return SUPLA_RESULTCODE_TRUE;
}

static int thermostat_config_recv(supla_channel_t *channel, TSD_ChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    TChannelConfig_HVAC             hvac_config;
    int                             rc = SUPLA_RESULTCODE_TRUE;

    if (config->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT)
        return SUPLA_RESULTCODE_TRUE;
    if (config->ConfigSize < sizeof(hvac_config)) {
        ESP_LOGE(TAG, "Received incomplete HVAC configuration");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(&hvac_config, config->Config, sizeof(hvac_config));
    if (!(hvac_config.Temperatures.Index & TEMPERATURE_HISTERESIS))
        return SUPLA_RESULTCODE_TRUE;

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    rc = thermostat_controller_set_hysteresis(
        &data->controller, hvac_config.Temperatures.Temperature[TEMPERATURE_HISTERESIS] / 100.0f);
    if (rc == 0)
        rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);

    if (rc != 0) {
        ESP_LOGE(TAG, "Rejected invalid HVAC hysteresis configuration");
        return ESP_ERR_INVALID_ARG;
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int thermostat_schedule_recv(supla_channel_t *channel, TSD_ChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    TChannelConfig_WeeklySchedule   schedule;
    int                             rc;

    if (config->ConfigType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE)
        return SUPLA_RESULTCODE_TRUE;
    if (config->ConfigSize < sizeof(schedule)) {
        ESP_LOGE(TAG, "Received incomplete thermostat schedule");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(&schedule, config->Config, sizeof(schedule));
    rc = validate_schedule(&schedule);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Received invalid thermostat schedule");
        return rc;
    }

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    data->schedule = schedule;
    data->schedule_valid = true;
    rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);
    return rc == ESP_OK ? SUPLA_RESULTCODE_TRUE : rc;
}

supla_channel_t *supla_thermostat_channel_create(const struct thermostat_channel_config *config)
{
    const supla_channel_config_t channel_config = {
        .type = SUPLA_CHANNELTYPE_HVAC,
        .supported_functions = SUPLA_BIT_FUNC_HVAC_THERMOSTAT,
        .default_function = SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
        .default_caption = "THERMOSTAT",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
        .on_channel_init = thermostat_channel_init,
        .on_set_value = thermostat_set_value,
        .on_config_set = thermostat_config_set,
        .on_config_recv = thermostat_config_recv,
        .on_sched_recv = thermostat_schedule_recv,
    };
    struct thermostat_channel_data     *data;
    struct thermostat_controller_config controller_config;
    struct thermostat_hal               controller_hal;
    esp_timer_create_args_t             timer_args = {
                    .name = "thermostat",
                    .dispatch_method = ESP_TIMER_TASK,
                    .callback = thermostat_timer,
    };
    uint32_t         poll_interval_ms;
    supla_channel_t *channel;

    if (!config || !config->hal.read_temperature_c || !config->hal.set_heating ||
        !is_thermometer_channel(config->thermometer_channel) || !isfinite(config->setpoint_c) ||
        (config->hysteresis_c != 0.0f &&
         (!isfinite(config->hysteresis_c) || config->hysteresis_c <= 0.0f ||
          config->hysteresis_c > 20.0f))) {
        ESP_LOGE(TAG, "Invalid thermostat channel configuration");
        return NULL;
    }

    channel = supla_channel_create(&channel_config);
    if (!channel)
        return NULL;

    data = calloc(1, sizeof(*data));
    if (!data) {
        supla_channel_free(channel);
        return NULL;
    }

    data->mutex = xSemaphoreCreateMutex();
    if (!data->mutex) {
        free(data);
        supla_channel_free(channel);
        return NULL;
    }

    data->thermometer_channel = config->thermometer_channel;
    data->manual_setpoint_c = config->setpoint_c;
    data->manual_enabled = config->initially_enabled;
    controller_config.setpoint_c = config->setpoint_c;
    controller_config.hysteresis_c = config->hysteresis_c == 0.0f ?
                                         THERMOSTAT_CONTROLLER_DEFAULT_HYSTERESIS_C :
                                         config->hysteresis_c;
    controller_hal.context = config->hal.context;
    controller_hal.read_temperature_c = config->hal.read_temperature_c;
    controller_hal.set_heating = config->hal.set_heating;

    if (thermostat_controller_init(&data->controller, &controller_hal, &controller_config) != 0) {
        ESP_LOGE(TAG, "Failed to initialize thermostat controller");
        vSemaphoreDelete(data->mutex);
        free(data);
        supla_channel_free(channel);
        return NULL;
    }

    supla_channel_set_data(channel, data);
    poll_interval_ms = config->poll_interval_ms ? config->poll_interval_ms :
                                                  THERMOSTAT_DEFAULT_POLL_INTERVAL_MS;
    timer_args.arg = channel;

    if (esp_timer_create(&timer_args, &data->timer) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create thermostat timer");
        vSemaphoreDelete(data->mutex);
        free(data);
        supla_channel_free(channel);
        return NULL;
    }
    data->poll_interval_ms = poll_interval_ms;
    return channel;
}

int supla_thermostat_channel_delete(supla_channel_t *channel)
{
    struct thermostat_channel_data *data;
    esp_err_t                       rc;

    if (!channel)
        return ESP_ERR_INVALID_ARG;

    data = supla_channel_get_data(channel);
    if (!data)
        return supla_channel_free(channel);

    rc = esp_timer_stop(data->timer);
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop thermostat timer: %s", esp_err_to_name(rc));
    rc = esp_timer_delete(data->timer);
    if (rc != ESP_OK)
        ESP_LOGE(TAG, "Failed to delete thermostat timer: %s", esp_err_to_name(rc));
    vSemaphoreDelete(data->mutex);
    free(data);
    return supla_channel_free(channel);
}

int supla_thermostat_channel_set_setpoint(supla_channel_t *channel, float setpoint_c)
{
    struct thermostat_channel_data *data;
    int                             rc;

    if (!channel || !isfinite(setpoint_c))
        return ESP_ERR_INVALID_ARG;

    data = supla_channel_get_data(channel);
    if (!data || xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    rc = thermostat_controller_set_setpoint(&data->controller, setpoint_c);
    if (rc == 0) {
        data->manual_setpoint_c = setpoint_c;
        data->manual_enabled = true;
        data->weekly_schedule = false;
        rc = update_channel_value(channel, data);
    } else
        rc = ESP_ERR_INVALID_ARG;
    xSemaphoreGive(data->mutex);
    return rc;
}

int supla_thermostat_channel_set_enabled(supla_channel_t *channel, bool enabled)
{
    struct thermostat_channel_data *data;
    int                             rc;

    if (!channel)
        return ESP_ERR_INVALID_ARG;

    data = supla_channel_get_data(channel);
    if (!data || xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    data->manual_enabled = enabled;
    data->weekly_schedule = false;
    rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);
    return rc;
}
