set(include_dirs "include" "tuya-mcu")
set(srcs "esp-tuya-mcu.c" 
         "tuya-mcu/tuya-mcu.c"
         "tuya-mcu/tuya-dp.c"
)

idf_component_register(
    SRCS "${srcs}"
    INCLUDE_DIRS "${include_dirs}"
    REQUIRES driver esp_event
)
