/**
 * @file usart_transport.c
 * @brief UART transport and log output for the MCU SDK port layer.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "usart_transport.h"
#include "ad_sdk.h"
#include "usart.h"

#define AD_SUCCESS 0
#define AD_ERROR   -1

#define UART_RX_BUFFER_SIZE 256
#define FRAME_RX_TIMEOUT_MS 20

static const char *TAG = "port_uart";

static struct {
    bool is_init;
} uart_state = {false};

static uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint16_t uart_rx_write_pos = 0;
static volatile uint16_t uart_rx_read_pos = 0;
static volatile uint32_t last_rx_time = 0;

/* STM32 HAL uses a non-const TX pointer although it never modifies TX data. */
static HAL_StatusTypeDef uart_transmit_readonly(UART_HandleTypeDef *huart,
                                                const void *data,
                                                uint16_t length,
                                                uint32_t timeout) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
    HAL_StatusTypeDef status = HAL_UART_Transmit(huart, (uint8_t *)data, length, timeout);
#pragma clang diagnostic pop
    return status;
}

int usart_transport_init(void) {
    if (uart_state.is_init) {
        AD_LOGW(TAG, "UART already initialized");
        return AD_SUCCESS;
    }

    uart_rx_write_pos = 0;
    uart_rx_read_pos = 0;
    last_rx_time = 0;
    memset(uart_rx_buffer, 0, sizeof(uart_rx_buffer));

    HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[0], 1);

    uart_state.is_init = true;
    AD_LOGI(TAG, "UART initialized (UART1)");

    return AD_SUCCESS;
}

int usart_transport_deinit(void) {
    if (!uart_state.is_init) {
        return AD_SUCCESS;
    }

    uart_state.is_init = false;
    AD_LOGI(TAG, "UART deinitialized");

    return AD_SUCCESS;
}

int usart_transport_tx(const uint8_t *data, uint16_t len) {
    if (!uart_state.is_init) {
        AD_LOGE(TAG, "UART not initialized");
        return AD_ERROR;
    }

    if (!data || len == 0) {
        AD_LOGE(TAG, "Invalid UART TX data or length");
        return AD_ERROR;
    }

    HAL_StatusTypeDef status = uart_transmit_readonly(&huart1, data, len, 1000U);
    if (status != HAL_OK) {
        AD_LOGE(TAG, "UART TX failed: %d", status);
        return AD_ERROR;
    }

    AD_LOGD(TAG, "UART TX: %d bytes", len);
    return AD_SUCCESS;
}

void usart_transport_rx_callback(void) {
    last_rx_time = HAL_GetTick();
    uart_rx_write_pos = (uart_rx_write_pos + 1) % UART_RX_BUFFER_SIZE;
    HAL_UART_Receive_IT(&huart1, &uart_rx_buffer[uart_rx_write_pos], 1);
}

void usart_transport_process(void) {
    if (!uart_state.is_init) {
        return;
    }

    uint16_t write_pos = uart_rx_write_pos;
    uint16_t read_pos = uart_rx_read_pos;

    if (write_pos == read_pos) {
        return;
    }

    uint16_t data_len;
    if (write_pos > read_pos) {
        data_len = write_pos - read_pos;
    } else {
        data_len = UART_RX_BUFFER_SIZE - read_pos + write_pos;
    }

    uint32_t current_time = HAL_GetTick();
    uint32_t time_since_last_rx = current_time - last_rx_time;

    if (data_len > 0 && time_since_last_rx < FRAME_RX_TIMEOUT_MS) {
        return;
    }

    if (data_len > 0) {
        uint8_t temp_buffer[UART_RX_BUFFER_SIZE];
        uint16_t copy_len = 0;

        for (uint16_t i = 0; i < data_len; i++) {
            temp_buffer[i] = uart_rx_buffer[(read_pos + i) % UART_RX_BUFFER_SIZE];
            copy_len++;
        }

        if (copy_len > 0) {
            AD_LOGD(TAG, "Processing %d bytes after %u ms idle", copy_len, (unsigned int)time_since_last_rx);
            {
                #define MAX_PRINT_BYTES 32
                uint16_t print_len = (copy_len > MAX_PRINT_BYTES) ? MAX_PRINT_BYTES : copy_len;
                char hexstr[MAX_PRINT_BYTES * 3 + 4] = {0};
                size_t pos = 0U;
                for (uint16_t i = 0; i < print_len; i++) {
                    int written = snprintf(&hexstr[pos], sizeof(hexstr) - pos,
                                           "%02X ", temp_buffer[i]);
                    if (written <= 0 || (size_t)written >= (sizeof(hexstr) - pos)) {
                        break;
                    }
                    pos += (size_t)written;
                }
                if (copy_len > MAX_PRINT_BYTES) {
                    snprintf(&hexstr[pos], sizeof(hexstr) - pos, "...");
                } else if (pos > 0U && hexstr[pos - 1U] == ' ') {
                    hexstr[pos - 1] = '\0';
                }
                AD_LOGD(TAG, "Received data: len=%d, data: %s", copy_len, hexstr);
                #undef MAX_PRINT_BYTES
            }
            ad_sdk_handle_rx_data(temp_buffer, copy_len);
            uart_rx_read_pos = (read_pos + copy_len) % UART_RX_BUFFER_SIZE;
        }
    }
}

void usart_transport_log_output(const char *message, uint32_t length) {
    static const uint8_t newline[] = "\r\n";
    uint32_t sent = 0U;
    while (sent < length) {
        uint32_t remaining = length - sent;
        uint16_t chunk = (remaining > UINT16_MAX) ? UINT16_MAX : (uint16_t)remaining;
        (void)uart_transmit_readonly(&huart2, &message[sent], chunk, HAL_MAX_DELAY);
        sent += chunk;
    }
    (void)uart_transmit_readonly(&huart2, newline, 2U, HAL_MAX_DELAY);
}
