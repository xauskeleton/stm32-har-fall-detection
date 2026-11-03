#include "buzzer.h"
#include "stm32f4xx_hal.h"

// TIM1 nằm trên APB2: PCLK2 = 90 MHz, clock timer = 180 MHz.
// Chia 180 cho ra tick 1 MHz, nên ARR = 1e6 / tần số - 1.
#define BUZZER_TIM_TICK_HZ  1000000u

// Còi 12 mm kêu rõ nhất quanh 2.7 kHz
#define BUZZER_FREQ_HZ      2700u

// 1 = còi chủ động (cấp điện là kêu): giữ PA8 mức cao liên tục khi kêu.
// 0 = còi thụ động: phát xung 50% ở BUZZER_FREQ_HZ.
#define BUZZER_ACTIVE       1

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
    // duty 0% = chân PA8 giữ mức thấp, còi im.
    // Còi chủ động: duty 100% (CCR1 > ARR) = PA8 mức cao liên tục.
    // Còi thụ động: duty 50% = sóng vuông ở BUZZER_FREQ_HZ.
    if (!on) {
        TIM1->CCR1 = 0;
    } else {
        TIM1->CCR1 = BUZZER_ACTIVE ? TIM1->ARR + 1 : (TIM1->ARR + 1) / 2;
    }
}

void Buzzer_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_TIM1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_8;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_PULLDOWN;   // giữ transistor tắt lúc reset, trước khi timer chạy
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio);

    TIM1->PSC = 180 - 1;                              // 180 MHz / 180 = 1 MHz
    TIM1->ARR = BUZZER_TIM_TICK_HZ / BUZZER_FREQ_HZ - 1;
    TIM1->CCR1 = 0;
    TIM1->CCMR1 = (6u << TIM_CCMR1_OC1M_Pos)          // PWM mode 1
                | TIM_CCMR1_OC1PE;
    TIM1->CCER = TIM_CCER_CC1E;
    TIM1->CR1 = TIM_CR1_ARPE;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->BDTR = TIM_BDTR_MOE;   // TIM1 là timer nâng cao, phải bật MOE thì mới ra chân
    TIM1->CR1 |= TIM_CR1_CEN;
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
