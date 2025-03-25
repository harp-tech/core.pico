cmake_minimum_required(VERSION 3.13)
#uncomment to print incoming and outgoing harp message stats.
#add_definitions(-DDEBUG_HARP_MSG_IN)
#add_definitions(-DDEBUG_HARP_MSG_OUT)

if(NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR
            "PICO_SDK_PATH was not specified.
             pico_sdk_init() must first be invoked.")
endif()

# commented out for now. Unclear how to use this with the GUI.
#option(DEBUG "If true, compile with added debug statements output to
#              uart at 921600 baud" OFF)

project(harp_core_rp2040)

# Use modern conventions like std::invoke
set(CMAKE_CXX_STANDARD 17)

add_library(core_registers
    src/core_registers.cpp
)

add_library(usb_desc
    src/usb_descriptors.c
    #${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/stdio_usb_descriptors.c
    ${PICO_SDK_PATH}/src/rp2_common/pico_stdio_usb/reset_interface.c
)

add_library(harp_core
    src/harp_core.cpp
)

add_library(harp_sync
    src/harp_synchronizer.cpp
)

add_library(harp_c_app
    src/harp_c_app.cpp
)

# Header file locations exposed with target scope for external projects.
target_include_directories(core_registers PUBLIC inc)
target_include_directories(usb_desc PUBLIC inc)
target_include_directories(harp_sync PUBLIC inc)
target_include_directories(harp_core PUBLIC inc)


target_link_libraries(usb_desc tinyusb_device pico_unique_id pico_stdlib pico_stdio_usb)# pico_usb_reset_interface)
target_link_libraries(harp_sync pico_stdlib)
target_link_libraries(harp_core core_registers pico_stdlib tinyusb_device usb_desc)
target_link_libraries(harp_c_app harp_core)

if(DEBUG)
    message(WARNING "Debug printf() messages from harp core to UART with baud \
            rate 921600.")
    # UART stdio for printf.
    # Additional libraries need to have stdio init also.
    pico_enable_stdio_uart(harp_core 1)
    pico_enable_stdio_uart(harp_sync 1)
endif()
