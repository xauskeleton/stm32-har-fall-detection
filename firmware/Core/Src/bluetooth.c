#include "bluetooth.h"
#include "board_config.h"
#include "fall_detect.h"
#include "uart_dma.h"
#include <stdio.h>
#include <string.h>

/* UART4 cau hinh bang thanh ghi truc tiep + DMA1 Stream 4 (Channel 4).
 * Hang doi vong 512 byte: du cho nhieu dong debug lien tiep. */

#define TXQ_SIZE  512u

static uint8_t txq[TXQ_SIZE];
static uart_dma_tx_t bt_dma = {
    .uart       = BT_UART,
    .dma        = DMA1,
    .stream     = DMA1_Stream4,
    .channel    = 4u,
    .stream_idx = 4u,
    .irqn       = DMA1_Stream4_IRQn,
    .buf        = txq,
    .size       = TXQ_SIZE,
};

static uint32_t t_power_on = 0;

/* Ten 12 hanh dong (tieng Anh) */
static const char *const ACTION_NAMES[12] = {
    "Walking", "Walking upstairs", "Walking downstairs",
    "Sitting", "Standing", "Laying",
    "Stand -> sit", "Sit -> stand", "Sit -> lie",
    "Lie -> sit", "Stand -> lie", "Lie -> stand",
};

static const char *action_name(int label)
{
    if (label < 1 || label > 12) return "Initializing";
    return ACTION_NAMES[label - 1];
}

static const char *state_name(int fall_state)
{
    if (fall_state == FALL_STATE_FALLEN) return "FALLEN";
    if (fall_state == FALL_STATE_SHOCK)  return "SHOCK";
    return "IDLE";
}

static void uart_start(void)
{
    __HAL_RCC_UART4_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BT_TX_PIN | BT_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLUP;
    gpio.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    gpio.Alternate = BT_GPIO_AF;
    HAL_GPIO_Init(BT_TX_PORT, &gpio);

    BT_UART->CR1 = 0;
    BT_UART->CR2 = 0;                               /* 1 stop bit */
    BT_UART->CR3 = 0;
    /* Oversampling 16: BRR = PCLK1 / baud (45 MHz / 115200 = 390.6 -> 391) */
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    BT_UART->BRR = (pclk + BT_BAUD / 2u) / BT_BAUD;
    BT_UART->CR1 = USART_CR1_TE;                    /* 8N1, chi can gui */
    BT_UART->CR1 |= USART_CR1_UE;                   /* bat UART sau khi cau hinh xong */
}

static bool booted(void)
{
    return HAL_GetTick() - t_power_on >= BT_BOOT_MS;
}

void Bluetooth_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* Cap nguon HC-05 va giu bat suot */
    HAL_GPIO_WritePin(BT_PWR_PORT, BT_PWR_PIN, GPIO_PIN_SET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BT_PWR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BT_PWR_PORT, &gpio);

    uart_start();
    UART_DMA_Init(&bt_dma);
    t_power_on = HAL_GetTick();

    /* Tu kiem tra: in cau hinh UART4 ra USART1, roi gui thang 1 dong qua HC-05
     * (cho HC-05 khoi dong truoc). */
    printf("UART4 DMA: CR1=0x%04lX CR3=0x%04lX BRR=%lu (PCLK1=%lu) | PA MODER=0x%08lX AFRL=0x%08lX\r\n",
           (unsigned long)BT_UART->CR1, (unsigned long)BT_UART->CR3,
           (unsigned long)BT_UART->BRR, (unsigned long)HAL_RCC_GetPCLK1Freq(),
           (unsigned long)GPIOA->MODER, (unsigned long)GPIOA->AFR[0]);
    HAL_Delay(BT_BOOT_MS);
    UART_DMA_Write(&bt_dma, "HC-05 OK\r\n", 10u);
    printf("UART4: sent \"HC-05 OK\" via A0 (DMA1 Stream4)\r\n");
}

void Bluetooth_DMA_IRQHandler(void)
{
    UART_DMA_IRQHandler(&bt_dma);
}

void Bluetooth_Send(const char *s)
{
    if (!booted() || !s) return;
    size_t len = strlen(s);
    if (len > 0u) UART_DMA_Write(&bt_dma, s, (uint16_t)len);
}

void Bluetooth_LogWindow(int label, const fall_state_t *fs, float a_min, float a_max)
{
    char smin[10], smax[10], line[112];
    UART_FmtF2(smin, sizeof smin, a_min, 0);
    UART_FmtF2(smax, sizeof smax, a_max, 0);
    snprintf(line, sizeof line,
             "%s | %s | |a|%s..%s | r=%d g=%d l=%d t=%d s=%d u=%d\r\n",
             action_name(label), state_name(fs->state), smin, smax,
             fs->active_run, fs->guard, fs->laying, fs->timer,
             fs->by_shock, fs->shock_upright);
    Bluetooth_Send(line);
}
