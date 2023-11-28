#include "include/relay-channel.h"

#include <stdlib.h>
#include <esp_log.h>
#include <driver/gpio.h>

struct relay_channel_data {
	gpio_num_t gpio;
};

static int relay_set_value(supla_channel_t *ch, TSD_SuplaChannelNewValue *new_value)
{
	struct relay_channel_data *ch_data = supla_channel_get_data(ch);
	TRelayChannel_Value *relay_val = (TRelayChannel_Value*)new_value->value;

	supla_log(LOG_INFO,"switch set value %s", relay_val->hi ? "OFF" : "ON");
	gpio_set_level(ch_data->gpio,!relay_val->hi);
	return supla_channel_set_relay_value(ch,relay_val);
}

supla_channel_t *supla_relay_channel_create(gpio_num_t gpio)
{
	supla_channel_config_t relay_channel_config = {
		.type = SUPLA_CHANNELTYPE_RELAY,
		.supported_functions = 0xFF,
		.default_function = SUPLA_CHANNELFNC_POWERSWITCH,
		.flags = SUPLA_CHANNEL_FLAG_CHANNELSTATE,
		.on_set_value = relay_set_value
	};
	gpio_config_t gpio_conf = {
		.pin_bit_mask = (1 << gpio),
		.mode = GPIO_MODE_OUTPUT,
		.intr_type = GPIO_INTR_DISABLE
	};
	struct relay_channel_data *ch_data;

	supla_channel_t *ch = supla_channel_create(&relay_channel_config);
	if(!ch)
		return NULL;

	ch_data = malloc(sizeof(struct relay_channel_data));
	if(!ch_data){
		supla_channel_free(ch);
		return NULL;
	}

	ch_data->gpio = gpio;
	supla_channel_set_data(ch,ch_data);
	gpio_config(&gpio_conf);
	return ch;
}
