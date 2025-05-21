if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(WIN32)
  set(USERHOME $ENV{USERPROFILE})
else()
  set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if(EXISTS ${picoVscode})
  include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD
    pico
    CACHE STRING "Board type")

project(ppm_enc C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

pico_sdk_init()

add_executable(ppm_enc 
    ppm_enc.cpp
    ${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c
)

pico_generate_pio_header(ppm_enc ${CMAKE_CURRENT_LIST_DIR}/enc.pio)

pico_set_program_name(ppm_enc "ppm_enc")
pico_set_program_version(ppm_enc "0.1")

pico_enable_stdio_uart(ppm_enc 0)
pico_enable_stdio_usb(ppm_enc 1)

target_link_libraries(ppm_enc PUBLIC 
    pico_stdlib
    hardware_pio
    pico_unique_id 
    tinyusb_device
    tinyusb_board
)

target_include_directories(ppm_enc PRIVATE ${CMAKE_CURRENT_LIST_DIR})

pico_add_extra_outputs(ppm_enc)
