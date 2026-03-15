cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2_w CACHE STRING "Target board type")
endif()

if (NOT DEFINED PICO_TOOLCHAIN_PATH)
    find_program(ARM_NONE_EABI_GCC arm-none-eabi-gcc)
    if (NOT ARM_NONE_EABI_GCC AND DEFINED ENV{HOME})
        file(GLOB ARDUINO_RP2040_GCC_BINS "$ENV{HOME}/.arduino15/packages/rp2040/tools/pqt-gcc/*/bin")
        foreach(CANDIDATE_BIN IN LISTS ARDUINO_RP2040_GCC_BINS)
            if (EXISTS "${CANDIDATE_BIN}/arm-none-eabi-gcc")
                set(PICO_TOOLCHAIN_PATH "${CANDIDATE_BIN}" CACHE PATH "Path containing arm-none-eabi toolchain binaries" FORCE)
                message(STATUS "Using Arduino RP2040 toolchain from ${PICO_TOOLCHAIN_PATH}")
                break()
            endif()
        endforeach()
    endif()
endif()

include(pico_sdk_import.cmake)

project(picokey C CXX ASM)

pico_sdk_init()

set(BTSTACK_EXAMPLE_PATH ${PICO_SDK_PATH}/lib/btstack/example)
set(BTSTACK_EXAMPLE_SOURCE ${BTSTACK_EXAMPLE_PATH}/hog_boot_host_demo.c)

add_executable(picokey
    src/main.c
    src/compat_stdio.c
    src/retarget_locks.c
    src/picokey_usb_hid.c
    src/picokey_pairing_monitor.c
    src/usb_descriptors.c
    src/btstack_hooks.c
    ${BTSTACK_EXAMPLE_SOURCE}
)

target_include_directories(picokey PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
    ${BTSTACK_EXAMPLE_PATH}
)

pico_enable_stdio_usb(picokey 0)
pico_enable_stdio_uart(picokey 0)

target_compile_definitions(picokey PRIVATE
    CYW43_LWIP=0
    PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS=0
)

set_source_files_properties(${BTSTACK_EXAMPLE_SOURCE} PROPERTIES
    COMPILE_DEFINITIONS "setvbuf=picokey_setvbuf;gatt_client_listen_for_characteristic_value_updates=picokey_gatt_client_listen_for_characteristic_value_updates"
    COMPILE_OPTIONS "-include;${CMAKE_CURRENT_LIST_DIR}/src/picokey_bt_hooks.h"
)

target_link_libraries(picokey
    pico_stdlib
    pico_btstack_ble
    pico_btstack_cyw43
    pico_cyw43_arch_threadsafe_background
    tinyusb_device
)

pico_add_extra_outputs(picokey)