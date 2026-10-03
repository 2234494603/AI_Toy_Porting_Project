set(app_core_dir ..)

set(incs
    include
    ${app_core_dir}/AI_Service/include
    ${app_core_dir}/Audio/include
    ${app_core_dir}/Bluetooth/include
    ${app_core_dir}/Display_Camera/include
    ${app_core_dir}/FreeRTOS/include
    ${app_core_dir}/Input/include
    ${app_core_dir}/LED/include
    ${app_core_dir}/Motor/include
    ${app_core_dir}/NFC/include
    ${app_core_dir}/Power/include
    ${app_core_dir}/Screen/include
    ${app_core_dir}/Sensor/include
    ${app_core_dir}/Storage/include
    ${app_core_dir}/System/include
    ${app_core_dir}/Wireless/include
)
set(srcs)

list(APPEND srcs
    src/main.c
    src/app_core.c
    ${app_core_dir}/AI_Service/src/app_ai_service.c
    ${app_core_dir}/Audio/src/app_audio.c
    ${app_core_dir}/Bluetooth/src/app_bluetooth.c
    ${app_core_dir}/Display_Camera/src/app_display_camera.c
    ${app_core_dir}/FreeRTOS/src/app_freertos_startup.c
    ${app_core_dir}/Input/src/app_input.c
    ${app_core_dir}/LED/src/app_led.c
    ${app_core_dir}/Motor/src/app_motor.c
    ${app_core_dir}/NFC/src/app_nfc.c
    ${app_core_dir}/Power/src/app_power.c
    ${app_core_dir}/Screen/src/app_screen.c
    ${app_core_dir}/Sensor/src/app_sensor.c
    ${app_core_dir}/Storage/src/app_storage.c
    ${app_core_dir}/System/src/app_system.c
    ${app_core_dir}/Wireless/src/app_wireless.c
)

if (CONFIG_SYS_CPU0)
list(APPEND srcs
    src/audio_para.c
)
endif()

if (CONFIG_OVERRIDE_FLASH_PARTITION)
    list(APPEND srcs
        src/vendor_flash.c
    )
endif()

armino_component_register(SRCS "${srcs}" 
    INCLUDE_DIRS "${incs}" 
    PRIV_REQUIRES bk_init lwip_intf_v2_1 VolcEngineRTCLite media_service multimedia avdk_utils mbedtls json wanson 
    bk_wanson_asr bk_factory_config bk_nfc bk_app_event bk_countdown bk_led_blink bk_motor bk_boarding_service 
    bk_smart_config bk_bt bk_key_app asr audio_engine video_engine network_transfer
    dual_screen_avi_play lvgl
)
