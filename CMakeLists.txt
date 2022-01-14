# Generated Cmake Pico project file
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(picoditdah C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(picoditdah
    ${CMAKE_CURRENT_LIST_DIR}/src/picoditdah.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/winkeyer_parser.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/cw_generator.cpp
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_descriptors.c
    ${CMAKE_CURRENT_LIST_DIR}/src/usb_devices.cpp
    ${CMAKE_CURRENT_LIST_DIR}/button-debouncer/button_debounce.cpp

    )

pico_generate_pio_header(picoditdah ${CMAKE_CURRENT_LIST_DIR}/button-debouncer/button_debounce.pio)

pico_set_program_name(picoditdah "picoditdah")
pico_set_program_version(picoditdah "0.1")

pico_enable_stdio_uart(picoditdah 1)
pico_enable_stdio_usb(picoditdah 0)

# Add the standard library to the build
target_link_libraries(picoditdah pico_stdlib tinyusb_device tinyusb_board hardware_pio)
target_include_directories(picoditdah PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

pico_add_extra_outputs(picoditdah)