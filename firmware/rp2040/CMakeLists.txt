# SPDX-License-Identifier: GPL-3.0-or-later
# USB_2_573's firmware for an RP2040 (docs/BUILDING.md):
#   cmake -S firmware/rp2040 -B build/rp2040 -DPICO_SDK_PATH=/path/to/pico-sdk   (or -DPICO_SDK_FETCH_FROM_GIT=ON)
#   cmake --build build/rp2040        ->  build/rp2040/usb_2_573.uf2
# -DPICO_BOARD=pico (the default) | pico2 | any RP2040/RP2350 board with the Pico's pins.
# -DU573_ANALOG_CHANNELS=4 for a bare RP2040 whose GPIO29 is the fourth analog channel.
cmake_minimum_required(VERSION 3.13)
include(pico_sdk_import.cmake)
project(usb_2_573 C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

add_executable(usb_2_573 main.c usb_descriptors.c ../core/u573_core.c)
target_include_directories(usb_2_573 PRIVATE ${CMAKE_CURRENT_LIST_DIR} ../core)
target_compile_options(usb_2_573 PRIVATE -Wall -Wextra)
if(DEFINED U573_ANALOG_CHANNELS)
  target_compile_definitions(usb_2_573 PRIVATE U573_ANALOG_CHANNELS=${U573_ANALOG_CHANNELS})
endif()
target_link_libraries(usb_2_573 PRIVATE pico_stdlib pico_unique_id pico_bootrom hardware_spi hardware_adc hardware_watchdog
                      tinyusb_device tinyusb_board)
pico_enable_stdio_usb(usb_2_573 0)
pico_enable_stdio_uart(usb_2_573 1)
pico_add_extra_outputs(usb_2_573)
