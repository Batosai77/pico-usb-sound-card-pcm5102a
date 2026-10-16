cmake_minimum_required(VERSION 3.13)

# TinyUSB 0.21+ (UAC1 untuk Windows 7); di-clone oleh build.sh ke ./tinyusb
if(NOT PICO_TINYUSB_PATH)
    set(PICO_TINYUSB_PATH ${CMAKE_CURRENT_LIST_DIR}/tinyusb)
endif()

include(pico_sdk_import.cmake)

project(pico_usb_sound_card C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Inisialisasi Pico SDK
pico_sdk_init()

add_executable(pico_usb_sound_card
    main.c
    audio_i2s.c
    usb_descriptors.c
)

# Kompilasi PIO program audio_i2s.pio menjadi header C (audio_i2s.pio.h)
pico_generate_pio_header(pico_usb_sound_card ${CMAKE_CURRENT_LIST_DIR}/audio_i2s.pio)

# Sertakan direktori saat ini agar header dapat ditemukan
target_include_directories(pico_usb_sound_card PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

# Tautkan pustaka TinyUSB dan Pico SDK yang diperlukan
target_link_libraries(pico_usb_sound_card
    pico_stdlib
    pico_unique_id
    tinyusb_device
    tinyusb_board
    hardware_pio
    hardware_clocks
    hardware_gpio
)

# Nonaktifkan stdio USB bawaan agar port USB sepenuhnya dikuasai oleh driver Audio DAC kita
pico_enable_stdio_usb(pico_usb_sound_card 0)
pico_enable_stdio_uart(pico_usb_sound_card 0)

# Otomatis menghasilkan berkas .uf2, .bin, dan .hex
pico_add_extra_outputs(pico_usb_sound_card)
