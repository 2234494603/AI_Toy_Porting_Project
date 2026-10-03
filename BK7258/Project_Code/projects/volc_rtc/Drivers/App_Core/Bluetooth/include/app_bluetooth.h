#pragma once

#include <stdbool.h>

/* Initializes Classic Bluetooth pairing and the HFP Hands-Free profile. */
void app_bluetooth_init(void);

/* User controls that can later be mapped to keys or UI actions. */
void app_bluetooth_enter_pairing(void);
void app_bluetooth_answer_call(void);
void app_bluetooth_reject_or_hang_up(void);

/* True while SCO owns the microphone and speaker. */
bool app_bluetooth_call_audio_active(void);
