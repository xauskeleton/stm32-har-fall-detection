#include "bluetooth.h"
#include "board_config.h"
#include "fall_detect.h"
#include <stdio.h>

/* UART4 cau hinh bang thanh ghi truc tiep, giong cach battery.c doc ADC.
 * Hang doi vong 256 byte: du cho vai dong trang thai. */

#define TXQ_SIZE  256u

static char     txq[TXQ_SIZE];
static uint16_t txq_head = 0, txq_tail = 0;

static bool     powered = false;
static uint32_t t_power_on = 0;
static uint32_t t_off_at = 0;        /* thoi diem duoc phep tat */
static uint32_t t_last_send = 0;
static int      prev_state = FALL_STATE_IDLE;

static void pwr_write(bool on)
{
    HAL_GPIO_WritePin(BT_PWR_PORT, BT_PWR_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/* Khi HC-05 tat, chan TX (PA0) dat ve analog: neu de TX o muc 1 (3.3 V) thi dong chay
 * nguoc qua chan RXD lam HC-05 "song do", vua ton dien vua de treo module. */
static void uart_pins(bool active)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BT_TX_PIN | BT_RX_PIN;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    if (active) {
        gpio.Mode = GPIO_MODE_AF_PP;
        gpio.Alternate = BT_GPIO_AF;
    } else {
        gpio.Mode = GPIO_MODE_ANALOG;
    }
    HAL_GPIO_Init(BT_TX_PORT, &gpio);
}

static void uart_start(void)
{
    __HAL_RCC_UART4_CLK_ENABLE();
    uart_pins(true);
    BT_UART->CR1 = 0;
    /* Oversampling 16: BRR = PCLK1 / baud (45 MHz / 9600 = 4687.5 -> 4688) */
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    BT_UART->BRR = (pclk + BT_BAUD / 2u) / BT_BAUD;
    BT_UART->CR2 = 0;                               /* 1 stop bit */
    BT_UART->CR3 = 0;
    BT_UART->CR1 = USART_CR1_TE | USART_CR1_UE;     /* 8N1, chi can gui */
}

static void uart_stop(void)
{
    BT_UART->CR1 = 0;
    __HAL_RCC_UART4_CLK_DISABLE();
    uart_pins(false);
}

static void power_on(void)
{
    if (powered) return;
    powered = true;
    t_power_on = HAL_GetTick();
    t_last_send = t_power_on;
    txq_head = txq_tail = 0;
    pwr_write(true);
    uart_start();
}

static void power_off(void)
{
    if (!powered) return;
    powered = false;
    uart_stop();
    pwr_write(false);
}

static bool booted(void)
{
    return powered && HAL_GetTick() - t_power_on >= BT_BOOT_MS;
}

void Bluetooth_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    pwr_write(false);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BT_PWR_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BT_PWR_PORT, &gpio);

    uart_pins(false);
}

void Bluetooth_Wake(uint32_t hold_ms)
{
    power_on();
    uint32_t until = HAL_GetTick() + hold_ms;
    if ((int32_t)(until - t_off_at) > 0) t_off_at = until;
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

bool Bluetooth_IsOn(void)
{
    return powered;
}

void Bluetooth_Update(int fall_state, int battery_pct)
{
    uint32_t now = HAL_GetTick();

    /* Bat theo trang thai te nga */
    if (fall_state == FALL_STATE_FALLEN) {
        Bluetooth_Wake(BT_HOLD_MS);                 /* con nam la con gia han */
        if (prev_state != FALL_STATE_FALLEN) t_last_send = 0;   /* bao ngay khi vao FALLEN */
    } else if (prev_state == FALL_STATE_FALLEN && booted()) {
        Bluetooth_Send("Da dung day\r\n");
    }
    prev_state = fall_state;

    if (!powered) return;

    /* Het thoi gian giu va khong con te nga -> tat, nhung cho gui het hang doi
     * va byte cuoi da ra khoi thanh ghi dich (TC) */
    if ((int32_t)(now - t_off_at) >= 0 && fall_state != FALL_STATE_FALLEN
            && txq_head == txq_tail && (BT_UART->SR & USART_SR_TC)) {
        power_off();
        return;
    }

    /* Gui trang thai dinh ky */
    if (booted() && now - t_last_send >= BT_SEND_MS) {
        t_last_send = now;
        const char *name = (fall_state == FALL_STATE_FALLEN) ? "TE NGA!"
                         : (fall_state == FALL_STATE_SHOCK)  ? "Va dap" : "Binh thuong";
        char line[48];
        if (battery_pct >= 0) snprintf(line, sizeof line, "%s | pin %d%%\r\n", name, battery_pct);
        else                  snprintf(line, sizeof line, "%s | nguon USB\r\n", name);
        Bluetooth_Send(line);
    }

    /* Day toi da 1 byte moi lan goi - khong chan vong lap */
    if (txq_tail != txq_head && (BT_UART->SR & USART_SR_TXE)) {
        BT_UART->DR = (uint8_t)txq[txq_tail];
        txq_tail = (uint16_t)((txq_tail + 1u) % TXQ_SIZE);
    }
}
