#
# Component Makefile
#
# This Makefile can be left empty. By default, it will take the sources in the 
# src/ directory, compile them and link them into lib(subdirectory_name).a 
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_IDF_TARGET_ESP8266
CPPFLAGS += -DCONFIG_IDF_TARGET_ESP8266=1
COMPONENT_SRCFILES := $(filter-out status-led-rgb.c,$(wildcard *.c))
else
COMPONENT_SRCFILES := $(wildcard *.c)
endif

COMPONENT_ADD_INCLUDEDIRS := include
