file(GLOB srcs "*.c")

if(CONFIG_IDF_TARGET_ESP8266)
    list(REMOVE_ITEM srcs "${CMAKE_CURRENT_SOURCE_DIR}/lampsmart-ble-channel.c")
endif()

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "include"
    REQUIRES esp-libsupla driver esp_timer pca9632 esp-tuya-mcu esp-lampsmart-ble
)
