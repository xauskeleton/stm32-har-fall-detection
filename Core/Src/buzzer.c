#include "buzzer.h"
#include "stm32f4xx_hal.h"

// Còi chủ động (cấp điện là kêu) ở PA8: chỉ cần bật/tắt chân, không cần timer
#define BUZZER_PORT  GPIOA
#define BUZZER_PIN   GPIO_PIN_8

// Nhịp lặp lại: mỗi bước là {1 = kêu / 0 = nghỉ, thời gian ms}.
// Muốn đổi nhịp thì chỉ cần sửa mảng này.
static const struct {
    uint8_t on;
    uint16_t dur_ms;
} rhythm[] = {
    {1, 100}, {0, 100},   // tít
    {1, 100}, {0, 100},   // tít
    {1, 350}, {0, 750},   // tèèè, rồi nghỉ
};
#define RHYTHM_LEN  (sizeof(rhythm) / sizeof(rhythm[0]))

static bool active = false;
static uint8_t step = 0;
static uint32_t step_start = 0;

static void set_output(uint8_t on)
{
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();

    set_output(0);   // ghi mức thấp trước khi bật output để còi không kêu lúc khởi động

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
    if (!active) {
        return;
    }

    uint32_t now = HAL_GetTick();
    if (now - step_start < rhythm[step].dur_ms) {
        return;
    }

    step_start = now;
    step = (uint8_t)((step + 1) % RHYTHM_LEN);
    set_output(rhythm[step].on);
}

bool Buzzer_IsOn(void)
{
    return active;
}
