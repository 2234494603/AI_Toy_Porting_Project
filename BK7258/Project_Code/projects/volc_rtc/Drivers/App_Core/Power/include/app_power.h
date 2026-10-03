#pragma once

/* Charger, battery ADC, external 3.3 V rail and deep-sleep policy. */
void app_power_battery_init(void);
void app_power_enter_deepsleep(void);
void app_power_handle_wakeup(void);
void app_power_init(void);
