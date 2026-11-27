# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

get_filename_component(ESP_IDF_LIB_COMPONENTS_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esp-idf-lib/components" ABSOLUTE)
file(GLOB ESP_IDF_LIB_COMPONENT_DIRS LIST_DIRECTORIES true
    "${ESP_IDF_LIB_COMPONENTS_DIR}/*")
list(REMOVE_ITEM ESP_IDF_LIB_COMPONENT_DIRS
    "${ESP_IDF_LIB_COMPONENTS_DIR}/led_strip")

set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/brd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../components
    ${ESP_IDF_LIB_COMPONENT_DIRS}
)

set(COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../main
    $ENV{IDF_PATH}/components 
    ${EXTRA_COMPONENT_DIRS} 
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32-S3-RELAY-6CH)
