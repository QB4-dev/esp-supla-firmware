#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp_device

EXTRA_COMPONENT_DIRS = $(PROJECT_PATH)/components/esp-idf-lib/components
EXCLUDE_COMPONENTS := max7219 ads130e08 mcp23x17 led_strip max31865 ls7366r max31855

include $(IDF_PATH)/make/project.mk