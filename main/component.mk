#
# Main Makefile. This is basically the same as a component makefile.
#

HTTPD_DATA_DIR = $(COMPONENT_PATH)/srv

COMPONENT_EMBED_FILES := $(shell  $(COMPONENT_PATH)/compress_webdata.sh $(HTTPD_DATA_DIR))

COMPONENT_EXTRA_CLEAN := $(COMPONENT_EMBED_FILES)
