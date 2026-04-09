# lampsmart_ble (ESP-IDF component)

ESP-IDF component for pairing and controlling LampSmart BLE CCT lights using BLE advertisements.

## Features

- Pair and unpair command support
- Turn on and turn off support
- Cold/warm white level control (0-255 or 0.0-1.0)
- Supports protocol variants: V3, V2, V1A, V1B

## Add to project

1. Copy this component into your ESP-IDF project under `components/lampsmart_ble`.
2. Enable Bluetooth in your ESP-IDF configuration.
3. Include the header and initialize the light controller.

## ESP32 quick start

1. In your ESP-IDF project, set the target:

```bash
idf.py set-target esp32
```

2. Initialize and pair in your `app_main()`.
3. Build and flash:

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Minimal usage

```c
#include "lampsmart_ble.h"

void app_main(void) {
  lampsmart_ble_t light = NULL;
  lampsmart_ble_config_t cfg = LAMPSMART_BLE_CONFIG_DEFAULT();

  cfg.group_id = 0x12345678;          // shared group id used by your lamp(s)
  cfg.variant = LAMPSMART_VARIANT_3;  // try other variants if needed
  cfg.tx_duration_ms = 1000;

  ESP_ERROR_CHECK(lampsmart_ble_init(&light, &cfg));

  // Put the lamp in pairing mode (usually power-cycle and pair within a short window)
  ESP_ERROR_CHECK(lampsmart_ble_pair(light));

  ESP_ERROR_CHECK(lampsmart_ble_set_levels(light, 180, 30));
  vTaskDelay(pdMS_TO_TICKS(3000));

  ESP_ERROR_CHECK(lampsmart_ble_turn_off(light));
}
```

## Notes

- This component sends non-connectable BLE advertisements and does not open a BLE GATT connection.
- Group addressing is used by this protocol, so multiple lamps in the same group can react together.
- If your lamp responds with swapped warm/cold channels, set `reversed_channels = true` in the config.
