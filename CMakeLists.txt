# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
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
# ====================================================================================
# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location (note this can come from
# environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension
# to work ==
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

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pico_ppm C CXX ASM)

set(PICO_CXX_ENABLE_EXCEPTIONS 1)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable СНАЧАЛА - перед генерацией заголовков PIO
add_executable(pico_ppm 
    pico_ppm.cpp
    usb_descriptors.c
)

# Генерация кода PIO - ПОСЛЕ объявления исполняемого файла
pico_generate_pio_header(pico_ppm ${CMAKE_CURRENT_LIST_DIR}/ppm.pio)

pico_set_program_name(pico_ppm "pico_ppm")
pico_set_program_version(pico_ppm "0.1")

# Настройка вывода через UART/USB
pico_enable_stdio_uart(pico_ppm 0)
pico_enable_stdio_usb(pico_ppm 1)

# Подключение библиотек
target_link_libraries(pico_ppm PUBLIC 
    pico_stdlib
    hardware_pio
    pico_unique_id 
    tinyusb_device
    tinyusb_board
)

# Add the standard include files to the build
target_include_directories(pico_ppm PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Генерация bin/uf2 файлов
pico_add_extra_outputs(pico_ppm)
