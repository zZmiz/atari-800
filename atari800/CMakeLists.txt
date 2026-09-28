cmake_minimum_required(VERSION 3.22)

set(HW_CONFIG 8 CACHE STRING "Hardware configuration")
set(USE_HSTX 1 CACHE STRING "Use RP2350 HSTX HDMI")
set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/pico-sdk" CACHE PATH "Pico SDK")
set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/Pico-PIO-USB" CACHE PATH "Pico PIO USB")
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/tinyusb" CACHE PATH "TinyUSB")
set(PICO_SHARED_PATH "${CMAKE_CURRENT_LIST_DIR}/../external/pico_shared" CACHE PATH "pico_shared")
set(NEEDS_LATEST_TINYUSB_SDK 0 CACHE BOOL "Use the pinned external TinyUSB checkout")
set(ENV{PICO_SDK_PATH} "${PICO_SDK_PATH}")

foreach(required
        "${PICO_SDK_PATH}/pico_sdk_init.cmake"
        "${PICO_PIO_USB_PATH}/src/pio_usb.c"
        "${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake"
        "${PICO_SHARED_PATH}/BoardConfigs.cmake")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "Missing dependency: ${required}. Run git submodule update --init --recursive.")
    endif()
endforeach()

list(APPEND PICO_BOARD_HEADER_DIRS "${PICO_SHARED_PATH}/boards")
include("${PICO_SHARED_PATH}/BoardConfigs.cmake")
if(NOT HW_CONFIG EQUAL 8 OR NOT PICO_BOARD STREQUAL "adafruit_fruit_jam")
    message(FATAL_ERROR "This release supports only Adafruit Fruit Jam (HW_CONFIG=8)")
endif()

set(FAMILY rp2040)
include("${PICO_TINYUSB_PATH}/hw/bsp/family_support.cmake")
include("${PICO_SDK_PATH}/pico_sdk_init.cmake")
project(picoAtari800 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()
check_and_add_pico_pio_usb_support()

add_subdirectory(drivers/fatfs)
add_subdirectory(drivers/sdcard)
add_subdirectory(drivers/nespad)
add_subdirectory(drivers/audio)
add_subdirectory(drivers/graphics)
add_subdirectory(drivers/hstx)
add_subdirectory("${PICO_SHARED_PATH}/drivers/tlv320dac3100"
                 "${CMAKE_BINARY_DIR}/tlv320dac3100")

file(GLOB_RECURSE ATARI800_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_LIST_DIR}/src/*.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp")
list(FILTER ATARI800_SOURCES EXCLUDE REGEX "/util_Wii_Joy\\.c$")

add_executable(${PROJECT_NAME}
    ${ATARI800_SOURCES}
    pico_shared/menu.cpp
    pico_shared/RomLister.cpp
    pico_shared/recentgames.cpp
    pico_shared/FrensFonts.cpp
    pico_shared/ffwrappers.cpp
    pico_shared/atari_adapter.cpp
    drivers/fruit_jam/graphics_bridge.c
    drivers/fruit_jam/psram_sram.c
    drivers/usb_input/gamepad.cpp
    drivers/usb_input/hid_app.cpp
    drivers/usb_input/usb_input.cpp
    drivers/usb_input/xinput/xinput_host.c)

target_include_directories(${PROJECT_NAME} PRIVATE
    drivers/usb_input drivers/usb_input/xinput src pico_shared
    drivers/fruit_jam drivers/psram)

target_compile_options(${PROJECT_NAME} PRIVATE
    -O2 -ffast-math -ffunction-sections -fdata-sections
    -Wall -Wextra -Wno-unused-parameter -Wno-unused-function
    -Wno-cast-function-type -Wno-maybe-uninitialized)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PACKAGE_VERSION="3.2.6-fruitjam"
    FRUIT_JAM_FRAME_PACING=1
    HW_CONFIG=${HW_CONFIG}
    PICO_STACK_SIZE=0x1000
    PICO_CORE1_STACK_SIZE=0x800
    PICO_FLASH_SIZE_BYTES=16777216
    PICO_DEFAULT_UART=${PICO_DEFAULT_UART}
    PICO_DEFAULT_UART_TX_PIN=${PICO_DEFAULT_UART_TX_PIN}
    PICO_DEFAULT_UART_RX_PIN=${PICO_DEFAULT_UART_RX_PIN}
    CFG_TUH_RPI_PIO_USB=${ENABLE_PIO_USB}
    PIO_USB_USE_PIO=${PIO_USB_USE_PIO}
    PICO_DEFAULT_PIO_USB_DP_PIN=1
    PICO_SECOND_PIO_USB_DP_PIN=-1
    PICO_PIO_USE_GPIO_BASE=1
    PRINTFBUTTONS=0
    GPIOHSTXD0=${GPIOHSTXD0}
    GPIOHSTXD1=${GPIOHSTXD1}
    GPIOHSTXD2=${GPIOHSTXD2}
    GPIOHSTXCK=${GPIOHSTXCK}
    GPIOHSTXINVERTED=${GPIOHSTXINVERTED}
    NOHSTXCLOCK=1
    DI_RING_BUFFER_SIZE=16
    HSTX_AUDIO_DI_HIGH_WATERMARK=12
    HSTX_AUDIO_RAW_RING_SIZE=2048
    SDCARD_SPI_BUS=${SD_SPI}
    SDCARD_PIN_SPI0_SCK=${SD_SCK}
    SDCARD_PIN_SPI0_MOSI=${SD_MOSI}
    SDCARD_PIN_SPI0_MISO=${SD_MISO}
    SDCARD_PIN_SPI0_CS=${SD_CS}
    NES_GPIO_CLK=${NES_CLK}
    NES_GPIO_LAT=${NES_LAT}
    NES_GPIO_DATA=${NES_DATA}
    NES_GPIO_CLK_1=${NES_CLK_1}
    NES_GPIO_LAT_1=${NES_LAT_1}
    NES_GPIO_DATA_1=${NES_DATA_1}
    WII_PIN_SDA=${WII_SDA}
    WII_PIN_SCL=${WII_SCL}
    WIIPAD_I2C=${WIIPAD_I2C}
    PICO_AUDIO_I2S_CLOCK_PIN_BASE=${PICO_AUDIO_I2S_CLOCK_PIN_BASE}
    PICO_AUDIO_I2S_DATA_PIN=${PICO_AUDIO_I2S_DATA_PIN}
    PICO_AUDIO_I2S_RESET_PIN=${PICO_AUDIO_I2S_RESET_PIN}
    PICO_AUDIO_I2S_INTERRUPT_PIN=${PICO_AUDIO_I2S_INTERRUPT_PIN}
    PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON=${PICO_AUDIO_I2S_INTERRUPT_IS_BUTTON}
    STEREO_SOUND=1 CONSOLE_SOUND=1 SERIO_SOUND=1 SOUND_THIN_API=1
    HAVE_OPENDIR=1)

target_link_libraries(${PROJECT_NAME} PRIVATE
    audio fatfs graphics nespad pico_hdmi sdcard tlv320dac3100
    pico_multicore pico_runtime pico_rand pico_stdlib
    tinyusb_board tinyusb_host tinyusb_pico_pio_usb
    hardware_clocks hardware_dma hardware_flash hardware_i2c hardware_interp
    hardware_pio hardware_pwm hardware_spi hardware_timer hardware_vreg)

pico_set_program_name(${PROJECT_NAME} "picoAtari800")
pico_set_program_version(${PROJECT_NAME} "3.2.6-fruitjam")
pico_enable_stdio_uart(${PROJECT_NAME} 1)
pico_enable_stdio_usb(${PROJECT_NAME} 0)
target_link_options(${PROJECT_NAME} PRIVATE -Wl,--print-memory-usage)
pico_add_extra_outputs(${PROJECT_NAME})
