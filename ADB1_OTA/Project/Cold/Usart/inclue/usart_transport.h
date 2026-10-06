/**
 * @file usart_transport.h
 * @brief UART-backed port functions used by the MCU SDK.
 */
#ifndef __USART_TRANSPORT_H__
#define __USART_TRANSPORT_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int usart_transport_init(void);
int usart_transport_deinit(void);
int usart_transport_tx(const uint8_t *data, uint16_t len);
void usart_transport_rx_callback(void);
void usart_transport_process(void);
void usart_transport_log_output(const char *message, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* __USART_TRANSPORT_H__ */
