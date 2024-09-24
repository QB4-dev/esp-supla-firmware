# SUPLA firmware based on ESP8266_RTOS/ESP-IDF SDK with esp-libsupla component

## compatibility

- [ESP8266 RTOS-SDK](https://github.com/espressif/ESP8266_RTOS_SDK)
- [ESP-IDF](https://github.com/espressif/ESP-IDF)

## How to start

#### Clone this repo with submodules:
`git clone --recursive https://github.com/QB4-dev/esp-supla-firmware`
#### To compile firmware for selected board go to `boards` directory and run:

`make` for ESP8266-based boards with and ESP8266_RTOS SDK 

`idf.py build` for ESP32-based boards with  and ESP-IDF SDK 
