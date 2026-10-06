#include "main.h"
#include "gpio.h"
#include "usart.h"
#include "ota_layout.h"
#include <string.h>

#define PROTO_HEADER             0xDA28U
#define PROTO_VERSION            0x01U
#define PROTO_OVERHEAD           9U
#define PROTO_MAX_FRAME          512U
#define OTA_MAX_CHUNK            240U

#define CMD_PREPARE              0x0D00U
#define CMD_PREPARE_RESP         0x0D01U
#define CMD_BOOT_READY           0x0D02U
#define CMD_DATA                 0x0D03U
#define CMD_DATA_ACK             0x0D04U
#define CMD_FINISH               0x0D05U
#define CMD_FINISH_RESP          0x0D06U
#define CMD_STATUS               0x0D07U

#define RET_OK                   0x00U
#define RET_PARAM                0x02U
#define RET_LENGTH               0x03U
#define RET_CHECKSUM             0x04U
#define RET_BUSY                 0x05U

static uint32_t received_bytes;
static uint32_t running_crc;
static uint8_t flash_prepared;
static uint32_t last_ready_tick;
static uint32_t last_led_tick;
static uint8_t led_index;

static uint16_t read_u16_be(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint32_t read_u32_be(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void write_u16_be(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void write_u32_be(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint16_t crc16_ccitt(const uint8_t *data, uint16_t len) {
    uint16_t crc = 0xFFFFU;
    while (len--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (crc & 0x8000U)
                    ? (uint16_t)(((uint32_t)crc << 1U) ^ 0x1021UL)
                    : (uint16_t)((uint32_t)crc << 1U);
        }
    }
    return crc;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, uint16_t len) {
    while (len--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320UL : 0UL);
        }
    }
    return crc;
}

static void send_frame(uint16_t cmd, const uint8_t *payload, uint16_t payload_len) {
    uint8_t frame[PROTO_MAX_FRAME];
    uint16_t total = (uint16_t)(PROTO_OVERHEAD + payload_len);
    uint16_t crc;
    if (total > sizeof(frame)) return;
    write_u16_be(&frame[0], PROTO_HEADER);
    frame[2] = PROTO_VERSION;
    write_u16_be(&frame[3], cmd);
    write_u16_be(&frame[5], payload_len);
    if (payload_len != 0U && payload != NULL) memcpy(&frame[7], payload, payload_len);
    crc = crc16_ccitt(frame, (uint16_t)(7U + payload_len));
    write_u16_be(&frame[7U + payload_len], crc);
    (void)HAL_UART_Transmit(&huart1, frame, total, 1000U);
}

static int program_u32(uint32_t address, uint32_t value) {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address,
                          (uint16_t)value) != HAL_OK) return -1;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address + 2U,
                          (uint16_t)(value >> 16)) != HAL_OK) return -1;
    return 0;
}

static int set_state(uint32_t state) {
    int result;
    HAL_FLASH_Unlock();
    result = program_u32(OTA_METADATA_ADDRESS + 8U, state);
    HAL_FLASH_Lock();
    return result;
}

static int write_metadata(uint32_t size, uint32_t crc32, uint32_t version_code) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    const uint32_t values[] = {
        OTA_METADATA_MAGIC, OTA_METADATA_FORMAT_VERSION, OTA_STATE_PENDING,
        size, crc32, version_code, 0xFFFFFFFFUL, 0xFFFFFFFFUL
    };
    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = OTA_METADATA_ADDRESS;
    erase.NbPages = 1U;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }
    for (uint32_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (program_u32(OTA_METADATA_ADDRESS + 4U * i, values[i]) != 0) {
            HAL_FLASH_Lock();
            return -1;
        }
    }
    HAL_FLASH_Lock();
    return 0;
}

static int erase_application(void) {
    FLASH_EraseInitTypeDef erase = {0};
    uint32_t error = 0;
    HAL_FLASH_Unlock();
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = OTA_APP_BASE_ADDRESS;
    erase.NbPages = OTA_APP_MAX_SIZE / OTA_FLASH_PAGE_SIZE;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
        HAL_FLASH_Lock();
        return -1;
    }
    HAL_FLASH_Lock();
    return set_state(OTA_STATE_DOWNLOADING);
}

static int program_data(uint32_t offset, const uint8_t *data, uint16_t len) {
    const ota_metadata_t *meta = (const ota_metadata_t *)OTA_METADATA_ADDRESS;
    uint32_t address = OTA_APP_BASE_ADDRESS + offset;
    uint16_t index = 0;
    if ((offset & 1U) != 0U || offset != received_bytes ||
        (offset + len) > meta->image_size || (offset + len) > OTA_APP_MAX_SIZE) return -1;

    HAL_FLASH_Unlock();
    while (index < len) {
        uint16_t halfword = data[index++];
        if (index < len) halfword |= (uint16_t)data[index++] << 8;
        else halfword |= 0xFF00U;
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, address, halfword) != HAL_OK) {
            HAL_FLASH_Lock();
            return -1;
        }
        address += 2U;
    }
    HAL_FLASH_Lock();
    running_crc = crc32_update(running_crc, data, len);
    received_bytes += len;
    return 0;
}

static uint8_t application_valid(void) {
    uint32_t stack = *(const uint32_t *)OTA_APP_BASE_ADDRESS;
    uint32_t reset = *(const uint32_t *)(OTA_APP_BASE_ADDRESS + 4U);
    return (stack >= OTA_SRAM_BASE_ADDRESS && stack <= OTA_SRAM_END_ADDRESS &&
            reset >= OTA_APP_BASE_ADDRESS && reset < OTA_METADATA_ADDRESS &&
            (reset & 1U) != 0U);
}

static void jump_to_application(void) {
    uint32_t stack = *(const uint32_t *)OTA_APP_BASE_ADDRESS;
    uint32_t reset = *(const uint32_t *)(OTA_APP_BASE_ADDRESS + 4U);
    void (*entry)(void) = (void (*)(void))reset;
    __disable_irq();
    HAL_UART_DeInit(&huart1);
    HAL_UART_DeInit(&huart2);
    HAL_DeInit();
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
    for (uint32_t i = 0; i < 8U; ++i) {
        NVIC->ICER[i] = 0xFFFFFFFFUL;
        NVIC->ICPR[i] = 0xFFFFFFFFUL;
    }
    SCB->VTOR = OTA_APP_BASE_ADDRESS;
    __set_MSP(stack);
    __enable_irq();
    entry();
}

static void send_ready(void) {
    const ota_metadata_t *meta = (const ota_metadata_t *)OTA_METADATA_ADDRESS;
    uint8_t payload[9];
    payload[0] = (meta->magic == OTA_METADATA_MAGIC) ? RET_OK : RET_PARAM;
    write_u32_be(&payload[1], received_bytes);
    write_u16_be(&payload[5], OTA_MAX_CHUNK);
    payload[7] = (uint8_t)(meta->state >> 8);
    payload[8] = (uint8_t)meta->state;
    send_frame(CMD_BOOT_READY, payload, sizeof(payload));
}

static void handle_frame(uint16_t cmd, const uint8_t *payload, uint16_t len) {
    const ota_metadata_t *meta = (const ota_metadata_t *)OTA_METADATA_ADDRESS;
    uint8_t response[5] = {RET_OK, 0, 0, 0, 0};

    if (cmd == CMD_PREPARE) {
        if (len < 12U) response[0] = RET_LENGTH;
        else if (read_u32_be(payload) == 0U || read_u32_be(payload) > OTA_APP_MAX_SIZE)
            response[0] = RET_PARAM;
        else if (write_metadata(read_u32_be(payload), read_u32_be(&payload[4]),
                                read_u32_be(&payload[8])) != 0)
            response[0] = RET_BUSY;
        else {
            received_bytes = 0U;
            running_crc = 0xFFFFFFFFUL;
            flash_prepared = 0U;
        }
        write_u32_be(&response[1], OTA_APP_MAX_SIZE);
        send_frame(CMD_PREPARE_RESP, response, sizeof(response));
        return;
    }

    if (cmd == CMD_DATA) {
        uint32_t offset;
        uint16_t data_len;
        if (len < 6U) response[0] = RET_LENGTH;
        else {
            offset = read_u32_be(payload);
            data_len = read_u16_be(&payload[4]);
            if (data_len > OTA_MAX_CHUNK || len != (uint16_t)(6U + data_len)) response[0] = RET_LENGTH;
            else if ((data_len & 1U) != 0U && (offset + data_len) != meta->image_size) response[0] = RET_PARAM;
            else if (!flash_prepared) {
                if (offset != 0U || erase_application() != 0) response[0] = RET_BUSY;
                else flash_prepared = 1U;
            }
            if (response[0] == RET_OK && program_data(offset, &payload[6], data_len) != 0)
                response[0] = RET_PARAM;
        }
        write_u32_be(&response[1], received_bytes);
        send_frame(CMD_DATA_ACK, response, sizeof(response));
        return;
    }

    if (cmd == CMD_FINISH) {
        uint32_t calculated_crc = running_crc ^ 0xFFFFFFFFUL;
        if (meta->magic != OTA_METADATA_MAGIC || received_bytes != meta->image_size)
            response[0] = RET_LENGTH;
        else if (calculated_crc != meta->image_crc32 || !application_valid())
            response[0] = RET_CHECKSUM;
        else if (set_state(OTA_STATE_VERIFIED) != 0)
            response[0] = RET_BUSY;
        send_frame(CMD_FINISH_RESP, response, 1U);
        if (response[0] == RET_OK) {
            HAL_Delay(100U);
            NVIC_SystemReset();
        }
    }
}

static void receive_and_process(void) {
    static uint8_t frame[PROTO_MAX_FRAME];
    static uint16_t used;
    uint8_t byte;
    if (HAL_UART_Receive(&huart1, &byte, 1U, 5U) != HAL_OK) return;
    if (used == 0U && byte != 0xDAU) return;
    if (used == 1U && byte != 0x28U) { used = (byte == 0xDAU) ? 1U : 0U; return; }
    frame[used++] = byte;
    if (used >= 7U) {
        uint16_t payload_len = read_u16_be(&frame[5]);
        uint16_t total = (uint16_t)(PROTO_OVERHEAD + payload_len);
        if (total > sizeof(frame)) { used = 0U; return; }
        if (used == total) {
            uint16_t expected = read_u16_be(&frame[total - 2U]);
            if (frame[2] == PROTO_VERSION && expected == crc16_ccitt(frame, total - 2U))
                handle_frame(read_u16_be(&frame[3]), &frame[7], payload_len);
            used = 0U;
        }
    }
}

static void update_led(void) {
    uint32_t now = HAL_GetTick();
    if ((now - last_led_tick) < 160U) return;
    last_led_tick = now;
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, (uint16_t)(GPIO_PIN_6 << led_index), GPIO_PIN_RESET);
    led_index = (uint8_t)((led_index + 1U) % 3U);
}

static void SystemClock_Config(void) {
    RCC_OscInitTypeDef osc = {0};
    RCC_ClkInitTypeDef clk = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
    osc.HSIState = RCC_HSI_ON;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMUL = RCC_PLL_MUL9;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) while (1) {}
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                    RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV2;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_2) != HAL_OK) while (1) {}
}

int main(void) {
    const ota_metadata_t *meta = (const ota_metadata_t *)OTA_METADATA_ADDRESS;
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();

    if (meta->magic == OTA_METADATA_MAGIC &&
        (meta->state == OTA_STATE_VERIFIED || meta->state == OTA_STATE_CONFIRMED) &&
        application_valid()) {
        jump_to_application();
    }

    received_bytes = 0U;
    running_crc = 0xFFFFFFFFUL;
    flash_prepared = 0U;
    last_ready_tick = HAL_GetTick() - 1000U;
    for (;;) {
        receive_and_process();
        update_led();
        if ((HAL_GetTick() - last_ready_tick) >= 1000U) {
            last_ready_tick = HAL_GetTick();
            send_ready();
        }
    }
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
