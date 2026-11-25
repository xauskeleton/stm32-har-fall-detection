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
static bool bt_ready = false;      /* UART4 + DMA da khoi tao xong */

/* Ten 12 hanh dong (tieng Anh). Len / xuong cau thang hien chung la "Walking" -
 * chi doi ten hien thi, model va may trang thai van phan biet 12 nhan. */
static const char *const ACTION_NAMES[12] = {
    "Walking", "Walking", "Walking",
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
    bt_ready = true;

    /* Tu kiem tra: cho HC-05 khoi dong xong roi gui thang 1 dong qua no. */
    HAL_Delay(BT_BOOT_MS);
    UART_DMA_Write(&bt_dma, "HC-05 OK\r\n", 10u);
}

void Bluetooth_DMA_IRQHandler(void)
{
    UART_DMA_IRQHandler(&bt_dma);
}

void Bluetooth_Write(const char *data, uint16_t len)
{
    if (!bt_ready || !booted() || !data || len == 0u) return;
    UART_DMA_Write(&bt_dma, data, len);
}

void Bluetooth_Send(const char *s)
{
    if (!s) return;
    Bluetooth_Write(s, (uint16_t)strlen(s));
}

void Bluetooth_LogWindow(int label, const fall_state_t *fs, float a_min, float a_max,
                         int battery_pct)
{
    char smin[10], smax[10], sbat[16], line[96];
    UART_FmtF2(smin, sizeof smin, a_min, 0);
    UART_FmtF2(smax, sizeof smax, a_max, 0);
    if (battery_pct >= 0) snprintf(sbat, sizeof sbat, "battery %d%%", battery_pct);
    else                  snprintf(sbat, sizeof sbat, "battery USB");
    snprintf(line, sizeof line, "%s | %s | amin=%s amax=%s | %s\r\n",
             action_name(label), state_name(fs->state), smin, smax, sbat);
    Bluetooth_Send(line);
}
