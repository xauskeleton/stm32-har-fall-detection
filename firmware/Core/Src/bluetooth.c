#include "bluetooth.h"
#include "board_config.h"
#include "fall_detect.h"
#include "uart_dma.h"
#include <math.h>
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
static uint32_t t_last_send = 0;
static int      prev_state = FALL_STATE_IDLE;

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
    if (fall_state == FALL_STATE_FALLEN) return "FALL";
    if (fall_state == FALL_STATE_SHOCK)  return "Impact";
    return "Normal";
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
    t_last_send = t_power_on;

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

static void fmt_vec3(char *dst, size_t sz, const float v[3])
{
    char x[10], y[10], z[10];
    UART_FmtF2(x, sizeof x, v ? v[0] : 0.0f, 1);
    UART_FmtF2(y, sizeof y, v ? v[1] : 0.0f, 1);
    UART_FmtF2(z, sizeof z, v ? v[2] : 0.0f, 1);
    snprintf(dst, sz, "(%s,%s,%s)", x, y, z);
}

void Bluetooth_Update(const bt_telemetry_t *t)
{
    uint32_t now = HAL_GetTick();

    /* Su kien te nga / dung day: bao ngay, khong cho chu ky */
    if (t->fall_state == FALL_STATE_FALLEN && prev_state != FALL_STATE_FALLEN) {
        char msg[80], smin[12], smax[12];
        UART_FmtF2(smin, sizeof smin, t->a_min, 0);
        UART_FmtF2(smax, sizeof smax, t->a_max, 0);
        snprintf(msg, sizeof msg,
                 "!!! FALL DETECTED (|a|%s..%sg, r=%d,g=%d,l=%d) !!!\r\n",
                 smin, smax, t->active_run, t->guard, t->laying);
        Bluetooth_Send(msg);
    } else if (t->fall_state != FALL_STATE_FALLEN && prev_state == FALL_STATE_FALLEN) {
        Bluetooth_Send("Recovered\r\n");
    }
    prev_state = t->fall_state;

    /* Dong trang thai + tham so debug dinh ky qua UART4 DMA */
    if (booted() && now - t_last_send >= BT_SEND_MS) {
        t_last_send = now;
        char msg[256], smin[12], smax[12], sbat[12];
        char v_grav[36], v_acc[36];
        UART_FmtF2(smin, sizeof smin, t->a_min, 0);
        UART_FmtF2(smax, sizeof smax, t->a_max, 0);
        UART_FmtF2(sbat, sizeof sbat, t->bat_v, 0);
        fmt_vec3(v_grav, sizeof v_grav, t->grav);
        fmt_vec3(v_acc,  sizeof v_acc,  t->acc_raw);

        int pa5_mv = (int)lroundf(t->bat_pa5_v * 1000.0f);
        int gx = t->gyro_dps ? (int)lroundf(t->gyro_dps[0]) : 0;
        int gy = t->gyro_dps ? (int)lroundf(t->gyro_dps[1]) : 0;
        int gz = t->gyro_dps ? (int)lroundf(t->gyro_dps[2]) : 0;

        int n;
        if (t->bat_pct >= 0) {
            n = snprintf(msg, sizeof msg,
                         "%s | %s | |a|%s..%s | g%s | bat %sV (%d%%)\r\n",
                         action_name(t->label), state_name(t->fall_state),
                         smin, smax, v_grav, sbat, t->bat_pct);
        } else {
            n = snprintf(msg, sizeof msg,
                         "%s | %s | |a|%s..%s | g%s | USB %sV\r\n",
                         action_name(t->label), state_name(t->fall_state),
                         smin, smax, v_grav, sbat);
        }
        if (n > 0 && (size_t)n < sizeof msg) {
            snprintf(msg + n, sizeof msg - (size_t)n,
                     "  dbg: acc%s | gyro(%+d,%+d,%+d dps) | sm(r=%d,g=%d,l=%d) | miss=%lu | adc=%lu(%d.%03dV)\r\n",
                     v_acc, gx, gy, gz,
                     t->active_run, t->guard, t->laying,
                     (unsigned long)t->missed,
                     (unsigned long)t->bat_adc, pa5_mv / 1000, pa5_mv % 1000);
        }
        Bluetooth_Send(msg);
    }
}
