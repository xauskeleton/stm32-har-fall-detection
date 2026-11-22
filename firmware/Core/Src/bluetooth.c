#include "bluetooth.h"
#include "board_config.h"
#include "fall_detect.h"
#include <stdio.h>

/* UART4 cau hinh bang thanh ghi truc tiep, giong cach battery.c doc ADC.
 * Hang doi vong 256 byte: du cho vai dong trang thai. */

#define TXQ_SIZE  256u

static char     txq[TXQ_SIZE];
static uint16_t txq_head = 0, txq_tail = 0;

static uint32_t t_power_on = 0;
static uint32_t t_last_send = 0;
static int      prev_state = FALL_STATE_IDLE;

/* Ten 12 hanh dong, khong dau de app nao cung hien dung */
static const char *const ACTION_NAMES[12] = {
    "Di bo", "Len cau thang", "Xuong cau thang", "Ngoi", "Dung", "Nam",
    "Dung -> ngoi", "Ngoi -> dung", "Ngoi -> nam", "Nam -> ngoi",
    "Dung -> nam", "Nam -> dung",
};

static const char *action_name(int label)
{
    if (label < 1 || label > 12) return "Dang khoi dong";
    return ACTION_NAMES[label - 1];
}

static const char *state_name(int fall_state)
{
    if (fall_state == FALL_STATE_FALLEN) return "TE NGA";
    if (fall_state == FALL_STATE_SHOCK)  return "Va dap";
    return "Binh thuong";
}

static void uart_start(void)
{
    __HAL_RCC_UART4_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BT_TX_PIN | BT_RX_PIN;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = BT_GPIO_AF;
    HAL_GPIO_Init(BT_TX_PORT, &gpio);

    BT_UART->CR1 = 0;
    /* Oversampling 16: BRR = PCLK1 / baud (45 MHz / 9600 = 4687.5 -> 4688) */
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    BT_UART->BRR = (pclk + BT_BAUD / 2u) / BT_BAUD;
    BT_UART->CR2 = 0;                               /* 1 stop bit */
    BT_UART->CR3 = 0;
    BT_UART->CR1 = USART_CR1_TE | USART_CR1_UE;     /* 8N1, chi can gui */
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
    t_power_on = HAL_GetTick();
    t_last_send = t_power_on;
}

void Bluetooth_Send(const char *s)
{
    if (!booted()) return;
    while (*s) {
        uint16_t next = (uint16_t)((txq_head + 1u) % TXQ_SIZE);
        if (next == txq_tail) return;               /* day hang doi -> bo phan con lai */
        txq[txq_head] = *s++;
        txq_head = next;
    }
}

void Bluetooth_Update(int label, int fall_state, int battery_pct)
{
    uint32_t now = HAL_GetTick();

    /* Su kien te nga / dung day: bao ngay, khong cho chu ky */
    if (fall_state == FALL_STATE_FALLEN && prev_state != FALL_STATE_FALLEN) {
        Bluetooth_Send("!!! TE NGA !!!\r\n");
    } else if (fall_state != FALL_STATE_FALLEN && prev_state == FALL_STATE_FALLEN) {
        Bluetooth_Send("Da dung day\r\n");
    }
    prev_state = fall_state;

    /* Dong trang thai dinh ky */
    if (booted() && now - t_last_send >= BT_SEND_MS) {
        t_last_send = now;
        char line[64];
        if (battery_pct >= 0)
            snprintf(line, sizeof line, "%s | %s | pin %d%%\r\n",
                     action_name(label), state_name(fall_state), battery_pct);
        else
            snprintf(line, sizeof line, "%s | %s | nguon USB\r\n",
                     action_name(label), state_name(fall_state));
        Bluetooth_Send(line);
    }

    /* Day toi da 1 byte moi lan goi - khong chan vong lap */
    if (txq_tail != txq_head && (BT_UART->SR & USART_SR_TXE)) {
        BT_UART->DR = (uint8_t)txq[txq_tail];
        txq_tail = (uint16_t)((txq_tail + 1u) % TXQ_SIZE);
    }
}
