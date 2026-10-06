#include "app_ota.h"
#include "ad_sdk.h"
#include "main.h"
#include "ota_layout.h"

#define OTA_PREPARE_MIN_PAYLOAD 12U
#define OTA_RESET_DELAY_MS      500U
#define OTA_CONFIRM_DELAY_MS    5000U
#define OTA_LED_STEP_MS         180U

static uint8_t reset_pending;
static uint8_t confirmation_pending;
static uint32_t state_started_at;
static uint32_t last_led_at;
static uint8_t led_phase;

static uint32_t read_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void leds_off(void) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8, GPIO_PIN_SET);
}

static void leds_all(uint8_t on) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8,
                      on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static int flash_program_u32(uint32_t address, uint32_t value) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, (uint16_t)value) != HAL_OK) return -1;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + 2U, (uint16_t)(value >> 16)) != HAL_OK) return -1;
    return 0;
}

static int write_pending_metadata(uint32_t size, uint32_t crc32, uint32_t version_code) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t page_error = 0;
    const uint32_t values[] = {OTA_METADATA_MAGIC, OTA_METADATA_FORMAT_VERSION,
        OTA_STATE_PENDING, size, crc32, version_code, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = OTA_METADATA_ADDRESS;
    erase.NbPages = 1;
    if (HAL_FLASHEx_Erase(&erase, &page_error) != HAL_OK) { HAL_FLASH_Lock(); return -1; }
    for (uint32_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (flash_program_u32(OTA_METADATA_ADDRESS + i * 4U, values[i]) != 0) {
            HAL_FLASH_Lock(); return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

static int confirm_running_image(void) {
    int result;
    HAL_FLASH_Unlock();
    result = flash_program_u32(OTA_METADATA_ADDRESS + 8U, OTA_STATE_CONFIRMED);
    HAL_FLASH_Lock();
    return result;
}

static void on_ota_message(uint16_t cmd, const uint8_t *payload, uint16_t payload_len) {
    uint8_t response[9];
    uint8_t result = AD_RET_OK;
    if (cmd != AD_CMD_MCU_OTA_PREPARE) return;
    if (payload == NULL || payload_len < OTA_PREPARE_MIN_PAYLOAD) result = AD_RET_LENGTH_ERROR;
    else {
        uint32_t size = read_u32_be(payload);
        if (size == 0U || size > OTA_APP_MAX_SIZE) result = AD_RET_PARAM_ERROR;
        else if (write_pending_metadata(size, read_u32_be(&payload[4]), read_u32_be(&payload[8])) != 0)
            result = AD_RET_BUSY;
    }
    response[0] = result;
    response[1] = (uint8_t)(OTA_APP_MAX_SIZE >> 24);
    response[2] = (uint8_t)(OTA_APP_MAX_SIZE >> 16);
    response[3] = (uint8_t)(OTA_APP_MAX_SIZE >> 8);
    response[4] = (uint8_t)OTA_APP_MAX_SIZE;
    response[5] = 0; response[6] = 0; response[7] = 0; response[8] = 240;
    (void)ad_sdk_send_ota_message(AD_CMD_MCU_OTA_PREPARE_RESP, response, sizeof(response));
    if (result == AD_RET_OK) {
        reset_pending = 1U; state_started_at = HAL_GetTick();
        last_led_at = state_started_at; led_phase = 0U;
    }
}

int app_ota_init(void) {
    const ota_metadata_t *metadata = (const ota_metadata_t *)OTA_METADATA_ADDRESS;
    reset_pending = 0U; confirmation_pending = 0U; leds_off();
    if (ad_sdk_register_ota_callback(on_ota_message) != AD_SDK_SUCCESS) return -1;
    if (metadata->magic == OTA_METADATA_MAGIC &&
        metadata->format_version == OTA_METADATA_FORMAT_VERSION &&
        metadata->state == OTA_STATE_VERIFIED) {
        confirmation_pending = 1U; state_started_at = HAL_GetTick();
        last_led_at = state_started_at; led_phase = 0U;
    }
    return 0;
}

void app_ota_process(void) {
    uint32_t now = HAL_GetTick();
    if (reset_pending) {
        if ((now - last_led_at) >= OTA_LED_STEP_MS) {
            last_led_at = now; led_phase ^= 1U;
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, led_phase ? GPIO_PIN_RESET : GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, led_phase ? GPIO_PIN_SET : GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_SET);
        }
        if ((now - state_started_at) >= OTA_RESET_DELAY_MS) NVIC_SystemReset();
        return;
    }
    if (confirmation_pending) {
        if ((now - last_led_at) >= OTA_LED_STEP_MS) {
            last_led_at = now; led_phase++; leds_all((led_phase & 1U) != 0U);
        }
        if ((now - state_started_at) >= OTA_CONFIRM_DELAY_MS) {
            (void)confirm_running_image(); confirmation_pending = 0U; leds_off();
        }
    }
}
