#ifndef _SUPLA_BINARY_SENSOR_H_
#define _SUPLA_BINARY_SENSOR_H_

#include <libsupla/channel.h>
#include <driver/gpio.h>

struct binary_sensor_config {
    gpio_num_t   gpio;
    int          default_function;    //SUPLA_CHANNELFNC_*
    unsigned int supported_functions; //SUPLA_BIT_FUNC_*
};

supla_channel_t *supla_binary_sensor_create(const struct binary_sensor_config *config);

#endif /* _SUPLA_CLICK_INPUT_H_ */
