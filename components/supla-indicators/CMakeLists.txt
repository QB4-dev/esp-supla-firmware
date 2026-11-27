file(GLOB srcs "*.c")
set(supla_indicators_requires esp-libsupla driver esp_timer color)

if(CONFIG_IDF_TARGET_ESP8266)
    list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/status-led-rgb.c")
else()
    list(APPEND supla_indicators_requires led_strip)
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES ${supla_indicators_requires}
)
