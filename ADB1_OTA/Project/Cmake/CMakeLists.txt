cmake_minimum_required(VERSION 3.20)

project(UART_LED LANGUAGES C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

set(STM32CUBE_F1_PATH "" CACHE PATH "Path to STM32Cube_FW_F1 root directory")
if(NOT STM32CUBE_F1_PATH AND DEFINED ENV{STM32CUBE_F1_PATH})
    set(STM32CUBE_F1_PATH "$ENV{STM32CUBE_F1_PATH}")
endif()
if(NOT STM32CUBE_F1_PATH AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/Drivers")
    set(STM32CUBE_F1_PATH "${CMAKE_CURRENT_LIST_DIR}")
endif()

set(STM32_HAL_DIR "${STM32CUBE_F1_PATH}/Drivers/STM32F1xx_HAL_Driver")
set(STM32_CMSIS_DEVICE_DIR "${STM32CUBE_F1_PATH}/Drivers/CMSIS/Device/ST/STM32F1xx")
set(STM32_CMSIS_CORE_DIR "${STM32CUBE_F1_PATH}/Drivers/CMSIS")

if(NOT EXISTS "${STM32_HAL_DIR}/Inc/stm32f1xx_hal.h")
    message(FATAL_ERROR
        "STM32CubeF1 HAL not found. Set STM32CUBE_F1_PATH to STM32Cube_FW_F1, "
        "for example: cmake -S Project/Cmake -B Project/Build "
        "-DSTM32CUBE_F1_PATH=/path/to/STM32Cube_FW_F1_V1.8.6")
endif()

set(LINKER_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/STM32F103XB_FLASH.ld")

set(APP_SOURCES
    ../Cold/Driver/src/main.c
    ../Cold/Driver/src/stm32f1xx_it.c
    ../Cold/Driver/src/stm32f1xx_hal_msp.c
    ../Cold/Driver/src/system_stm32f1xx.c
    ../Cold/Control/src/app_ctrl.c
    ../Cold/GPIO/src/gpio.c
    ../Cold/IOT/src/app_iot.c
    ../Cold/Time/src/soft_timer.c
    ../Cold/Time/src/system_tick.c
    ../Cold/Usart/src/usart.c
    ../Cold/Usart/src/usart_transport.c
    ../Cold/AI/src/app_ai.c
    ../Cold/Mode/src/app_modes.c
    ../Cold/Player/src/app_player.c
    ../Cold/WIFI/src/ad_sdk.c
)

set(STM32_HAL_SOURCES
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_cortex.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_dma.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_exti.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_flash.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_flash_ex.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_gpio.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_gpio_ex.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_pwr.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_rcc.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_rcc_ex.c
    ${STM32_HAL_DIR}/Src/stm32f1xx_hal_uart.c
)

set(STARTUP_SOURCE
    ${STM32_CMSIS_DEVICE_DIR}/Source/Templates/gcc/startup_stm32f103xb.s
)

add_executable(${PROJECT_NAME}
    ${APP_SOURCES}
    ${STM32_HAL_SOURCES}
    ${STARTUP_SOURCE}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ../Cold/Driver/inclue
    ../Cold/AI/inclue
    ../Cold/Control/inclue
    ../Cold/GPIO/inclue
    ../Cold/IOT/inclue
    ../Cold/Mode/inclue
    ../Cold/Player/inclue
    ../Cold/Time/inclue
    ../Cold/Usart/inclue
    ../Cold/WIFI/inclue
    ${STM32_HAL_DIR}/Inc
    ${STM32_HAL_DIR}/Inc/Legacy
    ${STM32_CMSIS_DEVICE_DIR}/Include
    ${STM32_CMSIS_CORE_DIR}/Include
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    USE_HAL_DRIVER
    STM32F103xB
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -ffunction-sections
    -fdata-sections
    $<$<COMPILE_LANGUAGE:C>:-Wall>
    $<$<COMPILE_LANGUAGE:C>:-Wextra>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unused-parameter>
)

target_link_options(${PROJECT_NAME} PRIVATE
    -mcpu=cortex-m3
    -mthumb
    -T${LINKER_SCRIPT}
    -specs=nano.specs
    -specs=nosys.specs
    -Wl,--gc-sections
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.map
    -Wl,--print-memory-usage
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    SUFFIX ".elf"
    LINK_DEPENDS "${LINKER_SCRIPT}"
)

if(CMAKE_OBJCOPY)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:${PROJECT_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.bin
        COMMENT "Generating HEX and BIN images"
    )
endif()

if(CMAKE_SIZE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${PROJECT_NAME}>
        COMMENT "Firmware size"
    )
endif()
