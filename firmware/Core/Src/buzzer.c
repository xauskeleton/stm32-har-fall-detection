#include "buzzer.h"
#include "board_config.h"

/* Nhip lap lai: {1 = keu / 0 = nghi, thoi gian ms}. Doi nhip thi sua mang nay. */
static const struct {
    uint8_t  on;
    uint16_t dur_ms;
} rhythm[] = {
    {1, 100}, {0, 100},   /* tit */
    {1, 100}, {0, 100},   /* tit */
    {1, 350}, {0, 750},   /* teee, roi nghi */
};
#define RHYTHM_LEN  (sizeof(rhythm) / sizeof(rhythm[0]))

static bool     active = false;
static uint8_t  step = 0;
static uint32_t step_start = 0;

static void set_output(uint8_t on)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    set_output(0);   /* ghi muc thap truoc khi bat output de coi khong keu luc khoi dong */

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BUZZER_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &gpio);
}

void Buzzer_Start(void)
{
    active = true;
    step = 0;
    step_start = HAL_GetTick();
    set_output(rhythm[0].on);
}

void Buzzer_Stop(void)
{
    active = false;
    set_output(0);
}

void Buzzer_Update(void)
{
    if (!active) return;

    uint32_t now = HAL_GetTick();
    if (now - step_start < rhythm[step].dur_ms) return;

    step_start = now;
    step = (uint8_t)((step + 1u) % RHYTHM_LEN);
    set_output(rhythm[step].on);
}

bool Buzzer_IsOn(void)
{
    return active;
}
