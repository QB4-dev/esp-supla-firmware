/*
 * Copyright (c) 2026 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "include/thermostat-channel.h"
#include "include/hvac-controller.h"

#include <esp-supla.h>
#include <math.h>
#include <nvs.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <esp_log.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static const char *TAG = "THERMOSTAT";
#define THERMOSTAT_DEFAULT_POLL_INTERVAL_MS 10000
#define THERMOSTAT_COUNTDOWN_INTERVAL_MS 1000
#define THERMOSTAT_MIN_VALID_EPOCH 1577836800
#define THERMOSTAT_NVS_STATE_VERSION 1

enum {
    HVAC_TEMPERATURE_INDEX_HYSTERESIS = 5,
    HVAC_TEMPERATURE_INDEX_ROOM_MIN = 10,
    HVAC_TEMPERATURE_INDEX_ROOM_MAX = 11,
};

struct thermostat_nvs_state {
    uint32_t                      version;
    TChannelConfig_HVAC           hvac_config;
    TChannelConfig_WeeklySchedule schedule;
    float                         manual_setpoint_c;
    bool                          hvac_config_valid;
    bool                          schedule_valid;
    bool                          schedule_seeded;
    bool                          weekly_schedule;
    bool                          manual_enabled;
};

struct thermostat_nvs_state_legacy {
    uint32_t                      version;
    TChannelConfig_HVAC           hvac_config;
    TChannelConfig_WeeklySchedule schedule;
    float                         manual_setpoint_c;
    bool                          hvac_config_valid;
    bool                          schedule_valid;
    bool                          weekly_schedule;
    bool                          manual_enabled;
};

struct thermostat_nvs_restore {
    struct thermostat_nvs_state        current;
    struct thermostat_nvs_state_legacy legacy;
};

struct thermostat_channel_data {
    struct thermostat_channel_hal hal;
    struct hvac_controller        controller;
    supla_channel_t              *thermometer_channel;
    supla_channel_t              *aux_thermometer_channel;
    uint8_t                        aux_thermometer_type;
    esp_timer_handle_t            timer;
    esp_timer_handle_t            countdown_timer;
    SemaphoreHandle_t             mutex;
    uint32_t                      poll_interval_ms;
    uint32_t                      seconds_left;
    struct thermostat_nvs_state   nvs_state;
    bool                          sensor_error;
    bool                          clock_error;
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
            if (!isfinite(setpoint) || setpoint < HVAC_CONTROLLER_MIN_SETPOINT_C ||
                setpoint > HVAC_CONTROLLER_MAX_SETPOINT_C)
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

static esp_err_t thermostat_nvs_state_store(supla_channel_t                *channel,
                                            struct thermostat_channel_data *data)
{
    esp_err_t rc =
        supla_esp_nvs_channel_state_store(channel, &data->nvs_state, sizeof(data->nvs_state));
    if (rc != ESP_OK)
        ESP_LOGE(TAG, "Failed to persist thermostat state: %s", esp_err_to_name(rc));
    return rc;
}

static int thermostat_publish_countdown(supla_channel_t                *channel,
                                        struct thermostat_channel_data *data)
{
    TTimerState_ExtendedValue timer_state = {};

    timer_state.RemainingTimeS = data->seconds_left;
    return supla_channel_set_timer_state_extvalue(channel, &timer_state) == SUPLA_RESULT_TRUE ?
               ESP_OK :
               ESP_FAIL;
}

static int thermostat_set_countdown(supla_channel_t *channel, struct thermostat_channel_data *data,
                                    uint32_t duration_seconds)
{
    esp_err_t rc = esp_timer_stop(data->countdown_timer);

    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to stop thermostat countdown: %s", esp_err_to_name(rc));
        return rc;
    }

    data->seconds_left = 0;
    if (duration_seconds > 0) {
        data->seconds_left = duration_seconds;
        rc = esp_timer_start_periodic(data->countdown_timer,
                                      THERMOSTAT_COUNTDOWN_INTERVAL_MS * 1000);
        if (rc != ESP_OK) {
            data->seconds_left = 0;
            ESP_LOGE(TAG, "Failed to start thermostat countdown: %s", esp_err_to_name(rc));
            thermostat_publish_countdown(channel, data);
            return rc;
        }
        ESP_LOGI(TAG, "Starting thermostat countdown for %u seconds", duration_seconds);
    }

    return thermostat_publish_countdown(channel, data);
}

static bool thermostat_nvs_state_is_valid(const struct thermostat_nvs_state *state)
{
    float hysteresis_c;

    if (state->version != THERMOSTAT_NVS_STATE_VERSION || !isfinite(state->manual_setpoint_c) ||
        state->manual_setpoint_c < HVAC_CONTROLLER_MIN_SETPOINT_C ||
        state->manual_setpoint_c > HVAC_CONTROLLER_MAX_SETPOINT_C)
        return false;

    if (state->schedule_valid && validate_schedule(&state->schedule) != ESP_OK)
        return false;

    if (state->hvac_config_valid &&
        (state->hvac_config.Temperatures.Index & TEMPERATURE_HISTERESIS)) {
        hysteresis_c =
            state->hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_HYSTERESIS] / 100.0f;
        if (!isfinite(hysteresis_c) || hysteresis_c <= 0.0f || hysteresis_c > 20.0f)
            return false;
    }

    return true;
}

static int thermostat_read_temperature_c(void *context, float *temperature_c)
{
    struct thermostat_channel_data *data = context;
    return data->hal.read_temperature_c(data->hal.user_ctx, temperature_c);
}

static int thermostat_set_output(void *context, enum hvac_controller_output output)
{
    struct thermostat_channel_data *data = context;

    if (output == HVAC_CONTROLLER_OUTPUT_COOLING)
        return ESP_ERR_NOT_SUPPORTED;
    return data->hal.set_heating(data->hal.user_ctx, output == HVAC_CONTROLLER_OUTPUT_HEATING);
}

static int publish_thermometer_value(supla_channel_t *channel, float temperature_c)
{
    supla_channel_config_t thermometer_config;

    if (supla_channel_get_config(channel, &thermometer_config) != SUPLA_RESULT_TRUE)
        return ESP_FAIL;
    if (thermometer_config.type != SUPLA_CHANNELTYPE_THERMOMETER &&
        thermometer_config.type != SUPLA_CHANNELTYPE_THERMOMETERDS18B20)
        return SUPLA_RESULT_TRUE;

    return supla_channel_set_double_value(channel, temperature_c);
}

static int update_channel_value(supla_channel_t *channel, struct thermostat_channel_data *data)
{
    struct tm  local_time;
    THVACValue value = {};
    float      measured_temperature_c = NAN;
    float      target_setpoint = data->nvs_state.manual_setpoint_c;
    bool       target_enabled = data->nvs_state.manual_enabled && !data->nvs_state.weekly_schedule;
    int        sensor_rc;
    int        thermometer_value_rc = SUPLA_RESULT_TRUE;
    int        rc;

    sensor_rc = data->hal.read_temperature_c(data->hal.user_ctx, &measured_temperature_c);
    data->sensor_error = sensor_rc != 0 || !isfinite(measured_temperature_c);
    data->clock_error = false;
    if (data->nvs_state.weekly_schedule) {
        if (!data->nvs_state.schedule_valid || !get_local_time(&local_time)) {
            data->clock_error = true;
            target_enabled = false;
        } else {
            int program_id = schedule_program_id(&data->nvs_state.schedule, local_time.tm_wday,
                                                 local_time.tm_hour, local_time.tm_min);
            if (program_id > 0) {
                const TWeeklyScheduleProgram *program =
                    &data->nvs_state.schedule.Program[program_id - 1];
                target_enabled = program->Mode == SUPLA_HVAC_MODE_HEAT;
                if (target_enabled)
                    target_setpoint = program->SetpointTemperatureHeat / 100.0f;
            } else {
                target_enabled = false;
            }
        }
    }

    rc = hvac_controller_set_heating_setpoint(&data->controller, target_setpoint);
    if (rc != 0)
        return ESP_ERR_INVALID_ARG;

    rc = hvac_controller_set_mode(&data->controller, target_enabled ? HVAC_CONTROLLER_MODE_HEAT :
                                                                      HVAC_CONTROLLER_MODE_OFF);
    if (rc != 0)
        return ESP_ERR_INVALID_ARG;
    if (!data->sensor_error) {
        thermometer_value_rc =
            publish_thermometer_value(data->thermometer_channel, measured_temperature_c);
        if (thermometer_value_rc != SUPLA_RESULT_TRUE)
            ESP_LOGE(TAG, "Failed to publish thermometer channel value: %d", thermometer_value_rc);
    } else if (sensor_rc != 0) {
        ESP_LOGE(TAG, "Thermometer read failed: %d", sensor_rc);
    } else {
        ESP_LOGE(TAG, "Thermometer returned an invalid temperature: %.2f C",
                 (double)measured_temperature_c);
    }

    rc = hvac_controller_update_with_temperature(&data->controller, measured_temperature_c);
    if (rc != 0)
        ESP_LOGE(TAG, "HVAC controller update failed: %d", rc);

    value.IsOn = target_enabled ? 1 : 0;
    value.Mode = target_enabled ? SUPLA_HVAC_MODE_HEAT : SUPLA_HVAC_MODE_OFF;
    value.SetpointTemperatureHeat = temperature_to_protocol(target_setpoint);
    value.Flags = SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET;
    if (data->nvs_state.weekly_schedule)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_WEEKLY_SCHEDULE;
    if (hvac_controller_is_heating(&data->controller))
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_HEATING;
    if (data->sensor_error)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_THERMOMETER_ERROR;
    if (data->clock_error)
        value.Flags |= SUPLA_HVAC_VALUE_FLAG_CLOCK_ERROR;

    if (supla_channel_set_value(channel, &value, sizeof(value)) != SUPLA_RESULT_TRUE)
        return ESP_FAIL;
    if (thermometer_value_rc != SUPLA_RESULT_TRUE)
        return ESP_FAIL;
    if (data->sensor_error)
        return sensor_rc != 0 ? sensor_rc : ESP_ERR_INVALID_ARG;
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

static void thermostat_countdown_timer_event(void *arg)
{
    supla_channel_t                *channel = arg;
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    int                             rc;

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return;

    if (data->seconds_left > 1) {
        data->seconds_left--;
        rc = thermostat_publish_countdown(channel, data);
    } else {
        rc = thermostat_set_countdown(channel, data, 0);
        data->nvs_state.manual_enabled = false;
        data->nvs_state.weekly_schedule = false;
        rc = update_channel_value(channel, data) == ESP_OK ? rc : ESP_FAIL;
        if (thermostat_nvs_state_store(channel, data) != ESP_OK)
            rc = ESP_FAIL;
        ESP_LOGI(TAG, "Thermostat countdown expired; turning heating off");
    }

    if (rc != ESP_OK)
        ESP_LOGE(TAG, "Thermostat countdown update failed: %s", esp_err_to_name(rc));
    xSemaphoreGive(data->mutex);
}

static int thermostat_channel_init(supla_channel_t *channel)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    struct thermostat_nvs_restore  *restore = calloc(1, sizeof(*restore));
    struct thermostat_nvs_state    *saved_state;
    struct thermostat_nvs_state_legacy *legacy_state;
    esp_err_t                       rc;

    if (!restore)
        return ESP_ERR_NO_MEM;

    saved_state = &restore->current;
    legacy_state = &restore->legacy;
    rc = supla_esp_nvs_channel_state_restore(channel, saved_state, sizeof(*saved_state));
    if (rc == ESP_ERR_INVALID_SIZE) {
        rc = supla_esp_nvs_channel_state_restore(channel, legacy_state, sizeof(*legacy_state));
        if (rc == ESP_OK && legacy_state->version == THERMOSTAT_NVS_STATE_VERSION) {
            saved_state->version = legacy_state->version;
            saved_state->hvac_config = legacy_state->hvac_config;
            saved_state->schedule = legacy_state->schedule;
            saved_state->manual_setpoint_c = legacy_state->manual_setpoint_c;
            saved_state->hvac_config_valid = legacy_state->hvac_config_valid;
            saved_state->schedule_valid = legacy_state->schedule_valid;
            saved_state->weekly_schedule = legacy_state->weekly_schedule;
            saved_state->manual_enabled = legacy_state->manual_enabled;
            ESP_LOGI(TAG, "Restored legacy thermostat state; schedule will be seeded if needed");
        }
    }
    if (rc == ESP_OK && thermostat_nvs_state_is_valid(saved_state)) {
        data->nvs_state = *saved_state;
        if (saved_state->hvac_config_valid &&
            (saved_state->hvac_config.Temperatures.Index & TEMPERATURE_HISTERESIS)) {
            float hysteresis_c =
                saved_state->hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_HYSTERESIS] /
                100.0f;
            rc = hvac_controller_set_hysteresis(&data->controller, hysteresis_c);
            if (rc != 0) {
                ESP_LOGE(TAG, "Failed to restore thermostat hysteresis");
                free(restore);
                return ESP_ERR_INVALID_STATE;
            }
        }
        rc = hvac_controller_set_heating_setpoint(&data->controller, saved_state->manual_setpoint_c);
        if (rc != 0) {
            ESP_LOGE(TAG, "Failed to restore thermostat setpoint");
            free(restore);
            return ESP_ERR_INVALID_STATE;
        }
        ESP_LOGI(TAG, "Restored thermostat configuration and schedule from NVS");
    } else if (rc == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGI(TAG, "No saved thermostat state; using defaults");
    } else if (rc != ESP_OK) {
        ESP_LOGW(TAG, "Unable to restore thermostat state: %s", esp_err_to_name(rc));
    } else {
        ESP_LOGW(TAG, "Ignoring invalid or incompatible thermostat state in NVS");
    }
    free(restore);

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_ARG)
        ESP_LOGW(TAG, "Initial thermostat update failed: %s", esp_err_to_name(rc));

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
        rc = hvac_controller_set_heating_setpoint(&data->controller, setpoint);
        if (rc != 0) {
            xSemaphoreGive(data->mutex);
            ESP_LOGE(TAG, "Rejected invalid setpoint: %.2f C", (double)setpoint);
            return ESP_ERR_INVALID_ARG;
        }
        data->nvs_state.manual_setpoint_c = setpoint;
    }

    switch (value->Mode) {
    case SUPLA_HVAC_MODE_NOT_SET:
        if (value->Flags & SUPLA_HVAC_VALUE_FLAG_SETPOINT_TEMP_HEAT_SET) {
            data->nvs_state.weekly_schedule = false;
            data->nvs_state.manual_enabled = true;
        }
        break;
    case SUPLA_HVAC_MODE_OFF:
        data->nvs_state.weekly_schedule = false;
        data->nvs_state.manual_enabled = false;
        break;
    case SUPLA_HVAC_MODE_HEAT:
        data->nvs_state.weekly_schedule = false;
        data->nvs_state.manual_enabled = true;
        break;
    case SUPLA_HVAC_MODE_CMD_TURN_ON:
        data->nvs_state.weekly_schedule = false;
        data->nvs_state.manual_enabled = true;
        break;
    case SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE:
        data->nvs_state.weekly_schedule = true;
        break;
    case SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL:
        data->nvs_state.weekly_schedule = false;
        data->nvs_state.manual_enabled = true;
        break;
    default:
        break;
    }

    rc = update_channel_value(channel, data);
    if (thermostat_nvs_state_store(channel, data) != ESP_OK && rc == ESP_OK)
        rc = ESP_FAIL;
    if (thermostat_set_countdown(channel, data, new_value->DurationSec) != ESP_OK && rc == ESP_OK)
        rc = ESP_FAIL;
    xSemaphoreGive(data->mutex);
    return rc == ESP_OK ? SUPLA_RESULTCODE_TRUE : rc;
}

static int thermostat_config_set(supla_channel_t *channel, TSDS_SetChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    supla_channel_config_t          thermometer_config;
    int thermometer_number = supla_channel_get_assigned_number(data->thermometer_channel);
    int aux_thermometer_number = 0;
    TChannelConfig_HVAC hvac_config = {};
    esp_err_t           store_rc;

    if (thermometer_number < 0 ||
        supla_channel_get_config(data->thermometer_channel, &thermometer_config) !=
            SUPLA_RESULT_TRUE ||
        !is_thermometer_channel(data->thermometer_channel)) {
        ESP_LOGE(TAG, "Thermometer channel must be added before the thermostat");
        return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
    }
    if (data->aux_thermometer_channel) {
        aux_thermometer_number =
            supla_channel_get_assigned_number(data->aux_thermometer_channel);
        if (aux_thermometer_number < 0 || aux_thermometer_number == thermometer_number ||
            !is_thermometer_channel(data->aux_thermometer_channel)) {
            ESP_LOGE(TAG, "Auxiliary thermometer must be added before the thermostat");
            return SUPLA_RESULTCODE_CHANNEL_CONFLICT;
        }
    }

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    if (data->nvs_state.hvac_config_valid)
        hvac_config = data->nvs_state.hvac_config;
    hvac_config.MainThermometerChannelNo = thermometer_number;
    hvac_config.AuxThermometerChannelNo = aux_thermometer_number;
    hvac_config.AuxThermometerType = data->aux_thermometer_type;
    hvac_config.AvailableAlgorithms = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
    hvac_config.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
    hvac_config.Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
    hvac_config.TemperatureSetpointChangeSwitchesToManualMode = 1;
    hvac_config.TemperatureControlType = SUPLA_HVAC_TEMPERATURE_CONTROL_TYPE_ROOM_TEMPERATURE;
    // hvac_config.ParameterFlags.MainThermometerChannelNoReadonly = 1;
    hvac_config.Temperatures.Index = TEMPERATURE_HISTERESIS | TEMPERATURE_ROOM_MIN |
                                     TEMPERATURE_ROOM_MAX;
    hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_HYSTERESIS] =
        temperature_to_protocol(data->controller.hysteresis_c);
    hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_ROOM_MIN] =
        temperature_to_protocol(HVAC_CONTROLLER_MIN_SETPOINT_C);
    hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_ROOM_MAX] =
        temperature_to_protocol(HVAC_CONTROLLER_MAX_SETPOINT_C);
    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;
    data->nvs_state.hvac_config = hvac_config;
    data->nvs_state.hvac_config_valid = true;
    store_rc = thermostat_nvs_state_store(channel, data);
    xSemaphoreGive(data->mutex);

    config->ConfigType = SUPLA_CONFIG_TYPE_DEFAULT;
    config->ConfigSize = sizeof(hvac_config);
    memcpy(config->Config, &hvac_config, sizeof(hvac_config));
    if (store_rc != ESP_OK)
        ESP_LOGE(TAG, "HVAC configuration was not persisted; defaults will be used after restart");
    return SUPLA_RESULTCODE_TRUE;
}

static int thermostat_config_recv(supla_channel_t *channel, TSD_ChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    TChannelConfig_HVAC             hvac_config;
    int                             rc = ESP_OK;
    esp_err_t                       persist_rc;

    if (config->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT)
        return SUPLA_RESULTCODE_TRUE;
    if (config->ConfigSize == 0) {
        ESP_LOGD(TAG, "No HVAC configuration received; keeping thermostat defaults");
        return SUPLA_RESULTCODE_TRUE;
    }
    if (config->ConfigSize < sizeof(hvac_config)) {
        ESP_LOGE(TAG, "Received incomplete HVAC configuration");
        return ESP_ERR_INVALID_SIZE;
    }

    memcpy(&hvac_config, config->Config, sizeof(hvac_config));
    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;
    if (hvac_config.Temperatures.Index & TEMPERATURE_HISTERESIS) {
        rc = hvac_controller_set_hysteresis(
            &data->controller,
            hvac_config.Temperatures.Temperature[HVAC_TEMPERATURE_INDEX_HYSTERESIS] / 100.0f);
    }
    if (rc != 0) {
        xSemaphoreGive(data->mutex);
        ESP_LOGE(TAG, "Rejected invalid HVAC hysteresis configuration");
        return ESP_ERR_INVALID_ARG;
    }

    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;
    data->nvs_state.hvac_config = hvac_config;
    data->nvs_state.hvac_config_valid = true;
    persist_rc = thermostat_nvs_state_store(channel, data);
    rc = persist_rc == ESP_OK ? update_channel_value(channel, data) : persist_rc;
    xSemaphoreGive(data->mutex);

    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to apply received HVAC configuration: %s", esp_err_to_name(rc));
        return rc;
    }
    return SUPLA_RESULTCODE_TRUE;
}

static int thermostat_schedule_recv(supla_channel_t *channel, TSD_ChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    TChannelConfig_WeeklySchedule  *schedule;
    int                             rc;

    ESP_LOGI(TAG, "Received schedule config: type=%d size=%d", config->ConfigType,
             config->ConfigSize);
    if (config->ConfigType == SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE) {
        ESP_LOGW(TAG, "Ignoring alternate schedule on heating-only thermostat");
        return SUPLA_RESULTCODE_TRUE;
    }
    if (config->ConfigType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE)
        return SUPLA_RESULTCODE_TRUE;
    if (config->ConfigSize == 0) {
        ESP_LOGI(TAG, "No weekly schedule configured");
        if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
            return ESP_FAIL;
        data->nvs_state.schedule_valid = false;
        data->nvs_state.schedule_seeded = true;
        data->nvs_state.weekly_schedule = false;
        rc = thermostat_nvs_state_store(channel, data);
        if (rc == ESP_OK)
            rc = update_channel_value(channel, data);
        xSemaphoreGive(data->mutex);
        return rc == ESP_OK ? SUPLA_RESULTCODE_TRUE : rc;
    }
    if (config->ConfigSize < sizeof(*schedule)) {
        ESP_LOGE(TAG, "Received incomplete thermostat schedule");
        return ESP_ERR_INVALID_SIZE;
    }

    schedule = malloc(sizeof(*schedule));
    if (!schedule)
        return ESP_ERR_NO_MEM;

    memcpy(schedule, config->Config, sizeof(*schedule));
    rc = validate_schedule(schedule);
    if (rc != ESP_OK) {
        ESP_LOGE(TAG, "Received invalid thermostat schedule");
        free(schedule);
        return rc;
    }

    for (unsigned int program = 0; program < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; ++program) {
        ESP_LOGI(TAG, "Schedule program %u: mode=%d heat=%d.%02d C", program + 1,
                 schedule->Program[program].Mode,
                 schedule->Program[program].SetpointTemperatureHeat / 100,
                 schedule->Program[program].SetpointTemperatureHeat % 100);
    }

    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE) {
        free(schedule);
        return ESP_FAIL;
    }
    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;
    data->nvs_state.schedule = *schedule;
    data->nvs_state.schedule_valid = true;
    data->nvs_state.schedule_seeded = true;
    ESP_LOGI(TAG, "Thermostat weekly schedule accepted");
    free(schedule);
    rc = thermostat_nvs_state_store(channel, data);
    if (rc == ESP_OK)
        rc = update_channel_value(channel, data);
    xSemaphoreGive(data->mutex);
    return rc == ESP_OK ? SUPLA_RESULTCODE_TRUE : rc;
}

static int thermostat_schedule_set(supla_channel_t *channel, TSDS_SetChannelConfig *config)
{
    struct thermostat_channel_data *data = supla_channel_get_data(channel);
    esp_err_t                       rc = ESP_OK;

    if (config->ConfigType != SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE)
        return SUPLA_RESULT_FALSE;
    if (xSemaphoreTake(data->mutex, portMAX_DELAY) != pdTRUE)
        return ESP_FAIL;

    if (!data->nvs_state.schedule_valid) {
        memset(&data->nvs_state.schedule, 0, sizeof(data->nvs_state.schedule));
        for (unsigned int program = 0; program < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE;
             ++program) {
            data->nvs_state.schedule.Program[program].Mode = SUPLA_HVAC_MODE_HEAT;
            data->nvs_state.schedule.Program[program].SetpointTemperatureHeat =
                (program + 1) * 500 + 500;
        }
        data->nvs_state.schedule_valid = true;
    }

    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;
    config->ConfigSize = sizeof(data->nvs_state.schedule);
    memcpy(config->Config, &data->nvs_state.schedule, sizeof(data->nvs_state.schedule));
    rc = thermostat_nvs_state_store(channel, data);
    xSemaphoreGive(data->mutex);
    if (rc != ESP_OK)
        return rc;

    ESP_LOGI(TAG, "Providing default OFF schedule with 10/15/20/25 C programs");
    return SUPLA_RESULT_TRUE;
}

supla_channel_t *supla_thermostat_channel_create(const struct thermostat_channel_config *config)
{
    const supla_channel_config_t channel_config = {
        .type = SUPLA_CHANNELTYPE_HVAC,
        .supported_functions = SUPLA_BIT_FUNC_HVAC_THERMOSTAT,
        .default_function = SUPLA_CHANNELFNC_HVAC_THERMOSTAT,
        .default_caption = "THERMOSTAT",
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED,
        .on_channel_init = thermostat_channel_init,
        .on_set_value = thermostat_set_value,
        .on_config_set = thermostat_config_set,
        .on_config_recv = thermostat_config_recv,
        .on_sched_recv = thermostat_schedule_recv,
        .on_sched_set = thermostat_schedule_set,
    };
    struct thermostat_channel_data *data;
    struct hvac_controller_config   controller_config;
    struct hvac_controller_hal      controller_hal;
    esp_timer_create_args_t         timer_args = {
                .name = "thermostat",
                .dispatch_method = ESP_TIMER_TASK,
                .callback = thermostat_timer,
    };
    esp_timer_create_args_t countdown_timer_args = {
        .name = "thermostat-off",
        .dispatch_method = ESP_TIMER_TASK,
        .callback = thermostat_countdown_timer_event,
    };
    uint32_t         poll_interval_ms;
    supla_channel_t *channel;

    if (!config || !config->hal.read_temperature_c || !config->hal.set_heating ||
        !is_thermometer_channel(config->thermometer_channel) || !isfinite(config->setpoint_c) ||
        (config->aux_thermometer_channel &&
         (!is_thermometer_channel(config->aux_thermometer_channel) ||
          config->aux_thermometer_channel == config->thermometer_channel ||
          config->aux_thermometer_type == SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET ||
          config->aux_thermometer_type > SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER)) ||
        (!config->aux_thermometer_channel &&
         config->aux_thermometer_type != SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET) ||
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
    data->aux_thermometer_channel = config->aux_thermometer_channel;
    data->aux_thermometer_type = config->aux_thermometer_type;
    data->hal = config->hal;
    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;
    data->nvs_state.manual_setpoint_c = config->setpoint_c;
    data->nvs_state.manual_enabled = config->initially_enabled;
    controller_config.mode = config->initially_enabled ? HVAC_CONTROLLER_MODE_HEAT :
                                                         HVAC_CONTROLLER_MODE_OFF;
    controller_config.heating_setpoint_c = config->setpoint_c;
    controller_config.cooling_setpoint_c = config->setpoint_c;
    controller_config.hysteresis_c =
        config->hysteresis_c == 0.0f ? HVAC_CONTROLLER_DEFAULT_HYSTERESIS_C : config->hysteresis_c;
    controller_hal.context = data;
    controller_hal.read_temperature_c = thermostat_read_temperature_c;
    controller_hal.set_output = thermostat_set_output;

    if (hvac_controller_init(&data->controller, &controller_hal, &controller_config) != 0) {
        ESP_LOGE(TAG, "Failed to initialize thermostat controller");
        vSemaphoreDelete(data->mutex);
        free(data);
        supla_channel_free(channel);
        return NULL;
    }
    data->nvs_state.manual_setpoint_c = config->setpoint_c;
    data->nvs_state.manual_enabled = config->initially_enabled;
    data->nvs_state.version = THERMOSTAT_NVS_STATE_VERSION;

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
    countdown_timer_args.arg = channel;
    if (esp_timer_create(&countdown_timer_args, &data->countdown_timer) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create thermostat countdown timer");
        esp_timer_delete(data->timer);
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
    rc = esp_timer_stop(data->countdown_timer);
    if (rc != ESP_OK && rc != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "Failed to stop thermostat countdown: %s", esp_err_to_name(rc));
    rc = esp_timer_delete(data->countdown_timer);
    if (rc != ESP_OK)
        ESP_LOGE(TAG, "Failed to delete thermostat countdown timer: %s", esp_err_to_name(rc));
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

    rc = hvac_controller_set_heating_setpoint(&data->controller, setpoint_c);
    if (rc == 0) {
        data->nvs_state.manual_setpoint_c = setpoint_c;
        data->nvs_state.manual_enabled = true;
        data->nvs_state.weekly_schedule = false;
        rc = update_channel_value(channel, data);
        if (thermostat_nvs_state_store(channel, data) != ESP_OK && rc == ESP_OK)
            rc = ESP_FAIL;
        if (thermostat_set_countdown(channel, data, 0) != ESP_OK && rc == ESP_OK)
            rc = ESP_FAIL;
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

    data->nvs_state.manual_enabled = enabled;
    data->nvs_state.weekly_schedule = false;
    rc = update_channel_value(channel, data);
    if (thermostat_nvs_state_store(channel, data) != ESP_OK && rc == ESP_OK)
        rc = ESP_FAIL;
    if (thermostat_set_countdown(channel, data, 0) != ESP_OK && rc == ESP_OK)
        rc = ESP_FAIL;
    xSemaphoreGive(data->mutex);
    return rc;
}
