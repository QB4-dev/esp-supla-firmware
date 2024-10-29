#include "ext-rgbw-channel.h"
#include <stdlib.h>
#include <esp_log.h>
#include <esp_timer.h>

struct rgbw_channel_data {
    i2c_dev_t *i2c_dev;
};

int supla_rgbw_channel_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
    struct rgbw_channel_data *ch_data = supla_channel_get_data(ch);
    TRGBW_Value              *rgbw = (TRGBW_Value *)new_value->value;
    uint8_t                   cb = rgbw->colorBrightness;

    uint8_t r = cb ? rgbw->R : 0x00;
    uint8_t g = cb ? rgbw->G : 0x00;
    uint8_t b = cb ? rgbw->B : 0x00;

    supla_log(LOG_INFO, "new RGBW val: R=%d G=%d B=%d CB=%d W=%d", rgbw->R, rgbw->G, rgbw->B,
              rgbw->colorBrightness, rgbw->brightness);
    pca9632_set_pwm_all(ch_data->i2c_dev, g, b, r, rgbw->colorBrightness);
    return supla_channel_set_rgbw_value(ch, rgbw);
}

supla_channel_t *supla_ext_rgbw_channel_create(const struct ext_rgbw_channel_config *ch_conf)
{
    supla_channel_config_t supla_channel_config = {
        .type = SUPLA_CHANNELTYPE_DIMMERANDRGBLED,
        .supported_functions = 0xFFFF,
        .default_function = SUPLA_CHANNELFNC_RGBLIGHTING,
        .flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE | SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED,
        .on_set_value = supla_rgbw_channel_set_value
    };
    struct rgbw_channel_data *ch_data;

    supla_channel_t *ch = supla_channel_create(&supla_channel_config);
    if (!ch)
        return NULL;

    ch_data = malloc(sizeof(struct rgbw_channel_data));
    if (!ch_data) {
        supla_channel_free(ch);
        return NULL;
    }

    ch_data->i2c_dev = ch_conf->pca9632;

    supla_channel_set_data(ch, ch_data);
    return ch;
}
