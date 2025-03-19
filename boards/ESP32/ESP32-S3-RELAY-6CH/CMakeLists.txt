# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(EXTRA_COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/brd
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../components
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../components/esp-idf-lib/components
)

set(COMPONENT_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../main
    $ENV{IDF_PATH}/components 
    ${EXTRA_COMPONENT_DIRS} 
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32-S3-RELAY-6CH)
