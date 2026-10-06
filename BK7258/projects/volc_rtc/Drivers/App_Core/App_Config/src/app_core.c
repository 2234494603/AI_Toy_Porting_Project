#include "app_core.h"

#include "app_ai_service.h"
#include "app_audio.h"
#include "app_bluetooth.h"
#include "app_display_camera.h"
#include "app_input.h"
#include "app_led.h"
#include "app_nfc.h"
#include "app_power.h"
#include "app_screen.h"
#include "app_storage.h"
#include "app_system.h"
#include "app_wireless.h"

void app_core_background_init(void)
{
    app_system_background_init();
    app_audio_background_init();
    app_display_camera_init();
    app_ai_service_init();
}

void app_core_boot_init(void)
{
    app_power_init();
    app_power_handle_wakeup();
    app_wireless_factory_init();
    app_led_init();

    app_audio_service_init();
    app_system_event_init();
    app_nfc_init();
    app_audio_frontend_init();
    app_audio_start_coprocessor();
    app_bluetooth_init();
    app_screen_startup();

    app_wireless_services_init();
    app_input_init();
    app_power_battery_init();
    app_storage_init();
}
