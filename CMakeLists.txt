# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_gamepad C CXX ASM)

pico_sdk_init()

add_executable(pico_gamepad
    main.cpp
)

target_include_directories(pico_gamepad PRIVATE ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(pico_gamepad
    pico_stdlib
    hardware_spi
    hardware_gpio
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(pico_gamepad 0)
pico_enable_stdio_uart(pico_gamepad 1)
pico_add_extra_outputs(pico_gamepad)
