idf_component_register(
    SRCS "src/lampsmart_ble.c"
        INCLUDE_DIRS "include"
        REQUIRES mbedtls
        PRIV_REQUIRES bt
)
