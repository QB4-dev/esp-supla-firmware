#
# Component Makefile
#
# Exclude the LampSmart BLE source on ESP8266 because the target does not
# provide the required BLE stack support.
#

ifdef CONFIG_IDF_TARGET_ESP8266
CPPFLAGS += -DCONFIG_IDF_TARGET_ESP8266=1
COMPONENT_SRCFILES := $(filter-out lampsmart-ble-channel.c,$(wildcard *.c))
else
COMPONENT_SRCFILES := $(wildcard *.c)
endif

COMPONENT_ADD_INCLUDEDIRS := include
