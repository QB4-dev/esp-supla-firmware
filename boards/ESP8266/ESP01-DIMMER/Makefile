#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp_supla_firmware

EXTRA_COMPONENT_DIRS += $(abspath brd)
EXTRA_COMPONENT_DIRS += $(abspath ../../../components)
EXTRA_COMPONENT_DIRS += $(abspath ../../../components/esp-idf-lib/components)

COMPONENT_DIRS := $(abspath ../../../main) ${IDF_PATH}/components ${EXTRA_COMPONENT_DIRS} 

EXCLUDE_COMPONENTS := max7219 ads130e08 mcp23x17 led_strip max31865 ls7366r max31855 encoder

include $(IDF_PATH)/make/project.mk

ifdef CONFIG_BSP_ESP01_DIMMER_v1_0
	PROJECT_NAME := "ESP01_DIMMER-v1.0"
endif

ifdef CONFIG_BSP_ESP01_DIMMER_v1_1
	PROJECT_NAME := "ESP01_DIMMER-v1.1"
endif
