#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := "NODE_MCU"

EXTRA_COMPONENT_DIRS += $(abspath brd)
EXTRA_COMPONENT_DIRS += $(abspath ../../../components)
EXTRA_COMPONENT_DIRS += $(abspath ../../../components/esp-idf-lib/components)

COMPONENT_DIRS := $(abspath ../../../main) ${IDF_PATH}/components ${EXTRA_COMPONENT_DIRS} 

EXCLUDE_COMPONENTS := max7219 ads130e08 mcp23x17 led_strip max31865 ls7366r max31855 encoder

include $(IDF_PATH)/make/project.mk
