#pragma once

/*
 * AIDK V1.0 product key contract from the board specification:
 *   S1 / GPIO13: short volume up, long enter provisioning
 *   S2 / GPIO12: short switch text/vision mode, long power off
 *   S3 / GPIO8 : short volume down, long factory reset
 *
 * POWER_ON remains assigned to the S2 double-click slot so the shared key
 * service registers GPIO12 as a deep-sleep wake source. The physical K1 reset
 * button is wired to the BK7258 reset pin and therefore needs no GPIO handler.
 */
#define KEY_DEFAULT_CONFIG_TABLE \
{ \
    { \
        .gpio_id = KEY_GPIO_13, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_UP, \
        .double_event = VOLUME_UP, \
        .long_event = CONFIG_NETWORK \
    }, \
    { \
        .gpio_id = KEY_GPIO_12, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = IR_MODE_SWITCH, \
        .double_event = POWER_ON, \
        .long_event = SHUT_DOWN \
    }, \
    { \
        .gpio_id = KEY_GPIO_8, \
        .active_level = LOW_LEVEL_TRIGGER, \
        .short_event = VOLUME_DOWN, \
        .double_event = VOLUME_DOWN, \
        .long_event = FACTORY_RESET \
    } \
}
