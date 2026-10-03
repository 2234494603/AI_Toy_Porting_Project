# Bluetooth module

This project module reuses `projects/common_components/bk_bt` rather than
copying the Bluetooth stack or HFP demo into `volc_rtc`.

- `app_bluetooth_init()` starts the Classic Bluetooth manager and HFP HF.
- The advertised name is `Project_Chen_<MAC suffix>`.
- HFP SCO owns the microphone and speaker while a call is active.
- The default project uses standard CVSD call audio to keep CPU0 FLASH usage
  within the current partition; mSBC can be re-enabled when more FLASH is made
  available.
- Volc RTC/AI is stopped before SCO audio starts and restored after SCO audio
  tasks release the endpoints.
- BLE Wi-Fi provisioning remains handled by the `Wireless` module.

Call control functions are exposed for a later key or LVGL binding. Phone-side
answer/hang-up also works without a local key binding.
