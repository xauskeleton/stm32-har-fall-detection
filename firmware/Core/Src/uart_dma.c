#include "uart_dma.h"
#include <math.h>
#include <stdio.h>

static uint32_t stream_shift(uint8_t stream_idx)
{
    static const uint8_t SHIFTS[4] = { 0u, 6u, 16u, 22u };
    return SHIFTS[stream_idx & 3u];
}

static uint32_t stream_isr(const uart_dma_tx_t *tx)
{
    return (tx->stream_idx < 4u) ? tx->dma->LISR : tx->dma->HISR;
}

static void stream_clear_flags(uart_dma_tx_t *tx)
{
    uint32_t mask = 0x3Du << stream_shift(tx->stream_idx);
    if (tx->stream_idx < 4u) tx->dma->LIFCR = mask;
    else                     tx->dma->HIFCR = mask;
}

static void start_next(uart_dma_tx_t *tx)
{
    if (tx->dma_len != 0u || tx->head == tx->tail) return;

    uint16_t head = tx->head;
    uint16_t tail = tx->tail;
    uint16_t chunk = (head > tail) ? (uint16_t)(head - tail)
                                   : (uint16_t)(tx->size - tail);
    tx->dma_len = chunk;

    if (tx->stream->CR & DMA_SxCR_EN) {
        tx->stream->CR &= ~DMA_SxCR_EN;
        while (tx->stream->CR & DMA_SxCR_EN) { }
    }
    stream_clear_flags(tx);
    tx->uart->SR = ~USART_SR_TC;

    tx->stream->PAR  = (uint32_t)&tx->uart->DR;
    tx->stream->M0AR = (uint32_t)&tx->buf[tail];
    tx->stream->NDTR = chunk;
    tx->stream->CR   = ((uint32_t)tx->channel << DMA_SxCR_CHSEL_Pos)
                     | DMA_SxCR_MINC
                     | DMA_SxCR_DIR_0
                     | DMA_SxCR_TCIE
                     | DMA_SxCR_EN;
}

void UART_DMA_Init(uart_dma_tx_t *tx)
{
    if (tx->dma == DMA1) __HAL_RCC_DMA1_CLK_ENABLE();
    else                 __HAL_RCC_DMA2_CLK_ENABLE();

    if (tx->stream->CR & DMA_SxCR_EN) {
        tx->stream->CR &= ~DMA_SxCR_EN;
        while (tx->stream->CR & DMA_SxCR_EN) { }
    }
    stream_clear_flags(tx);

    tx->head = 0u;
    tx->tail = 0u;
    tx->dma_len = 0u;
    tx->uart->CR3 |= USART_CR3_DMAT;

    HAL_NVIC_SetPriority(tx->irqn, 6, 0);
    HAL_NVIC_EnableIRQ(tx->irqn);
}

uint16_t UART_DMA_Write(uart_dma_tx_t *tx, const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t written = 0u;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    while (written < len) {
        uint16_t next = (uint16_t)((tx->head + 1u) % tx->size);
        if (next == tx->tail) break;
        tx->buf[tx->head] = p[written++];
        tx->head = next;
    }
    start_next(tx);

    if (!primask) __enable_irq();
    return written;
}

void UART_DMA_IRQHandler(uart_dma_tx_t *tx)
{
    uint32_t tcif = 1u << (stream_shift(tx->stream_idx) + 5u);
    if (stream_isr(tx) & tcif) {
        stream_clear_flags(tx);
        tx->tail = (uint16_t)((tx->tail + tx->dma_len) % tx->size);
        tx->dma_len = 0u;
        start_next(tx);
    }
}

void UART_FmtF2(char *dst, size_t sz, float v, int show_plus)
{
    int c = (int)lroundf(v * 100.0f);
    char sign = (c < 0) ? '-' : '+';
    int ac = (c < 0) ? -c : c;
    if (show_plus || c < 0)
        snprintf(dst, sz, "%c%d.%02d", sign, ac / 100, ac % 100);
    else
        snprintf(dst, sz, "%d.%02d", ac / 100, ac % 100);
}
