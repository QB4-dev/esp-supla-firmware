#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := esp_device

include $(IDF_PATH)/make/project.mk

EXCLUDE_COMPONENTS := max7219 mcp23x17 led_strip max31865 ls7366r max31855
