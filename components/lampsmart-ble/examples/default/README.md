# ESP32 LampSmart Example (ESP-IDF)

This example targets ESP32 and uses the `lampsmart_ble` component to pair and control BLE lights.

## 1. Prepare environment

- Install ESP-IDF and export environment variables.
- Set a variable so CMake can find this repository components directory:

```bash
export EXAMPLE_COMPONENTS_PATH=/absolute/path/to/esphome-lampsmart
```

## 2. Create dependency path

Create a symlink inside your ESP-IDF project to expose the component:

```bash
cd examples/esp32_lampsmart_idf
mkdir -p managed_components
ln -s "$EXAMPLE_COMPONENTS_PATH/idf_components/lampsmart_ble" managed_components/lampsmart_ble
```

(You can also copy the component into `components/lampsmart_ble` instead of symlinking.)

## 3. Build for ESP32

This example includes `sdkconfig.defaults` enabling Bluetooth/BLE/Bluedroid.
If you already built before adding defaults, run a clean reconfigure first:

```bash
idf.py fullclean
```

```bash
idf.py set-target esp32
idf.py build
```

## 4. Flash and monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

## Pairing note

Most LampSmart-compatible lamps require pairing shortly after power-on. Power-cycle the lamp, then run the firmware so `lampsmart_ble_pair()` executes in the pairing window.
