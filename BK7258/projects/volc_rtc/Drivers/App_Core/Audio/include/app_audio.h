#pragma once

#include <stdbool.h>

void app_audio_background_init(void);
void app_audio_frontend_init(void);
void app_audio_service_init(void);
void app_audio_start_coprocessor(void);

/* HFP uses a separate audio path. These calls hand the board microphone and
 * speaker to Bluetooth for the duration of an SCO call, then restore AI audio. */
bool app_audio_suspend_for_bluetooth(void);
void app_audio_resume_after_bluetooth(void);
