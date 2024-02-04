#
# Main Makefile. This is basically the same as a component makefile.
#

COMPONENT_EMBED_FILES+= srv/index.html
COMPONENT_EMBED_FILES+= srv/supla.css

COMPONENT_ADD_INCLUDEDIRS = bsp/include
COMPONENT_SRCDIRS += bsp
