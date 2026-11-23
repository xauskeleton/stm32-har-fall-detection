/* Truyen UART TX khong chan bang DMA + hang doi vong.
 * Dung chung cho USART1 (DMA2 Stream 7 Ch 4) va UART4 (DMA1 Stream 4 Ch 4).
 */
#ifndef UART_DMA_H
#define UART_DMA_H

#include "stm32f4xx_hal.h"
#include <stddef.h>
#include <stdint.h>

typedef struct {
    USART_TypeDef      *uart;
    DMA_TypeDef        *dma;
    DMA_Stream_TypeDef *stream;
    uint8_t             channel;      /* 0..7 */
    uint8_t             stream_idx;   /* 0..7 */
    IRQn_Type           irqn;
    uint8_t            *buf;
    uint16_t            size;
    volatile uint16_t   head;
    volatile uint16_t   tail;
    volatile uint16_t   dma_len;      /* so byte DMA dang day (0 = ranh) */
} uart_dma_tx_t;

void     UART_DMA_Init(uart_dma_tx_t *tx);
uint16_t UART_DMA_Write(uart_dma_tx_t *tx, const void *data, uint16_t len);
void     UART_DMA_IRQHandler(uart_dma_tx_t *tx);
void     UART_FmtF2(char *dst, size_t sz, float v, int show_plus);

#endif /* UART_DMA_H */
