#include "rgb_led.h"
#include "board_config.h"

/* Cuc am chung, dieu khien bang PWM phan cung (~1.96 kHz, 0..255):
 *   PB3 = TIM2_CH2 (AF1) - Do
 *   PB4 = TIM3_CH1 (AF2) - Xanh la
 *   PB5 = TIM3_CH2 (AF2) - Xanh duong */
#define PWM_ARR  254u   /* APB1 timer 90 MHz / (179 + 1) / (254 + 1) = 1961 Hz */

static void init_pwm_pin(GPIO_TypeDef *port, uint16_t pin, uint8_t af)
{
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_AF_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    gpio.Alternate = af;
    HAL_GPIO_Init(port, &gpio);
}

void RGB_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    /* PB3 (SWO) va PB4 (NJTRST) mac dinh la chan JTAG. Nap/debug SWD chi can
     * PA13/PA14 nen doi sang AF timer van binh thuong. */
    init_pwm_pin(RGB_R_PORT, RGB_R_PIN, GPIO_AF1_TIM2);
    init_pwm_pin(RGB_G_PORT, RGB_G_PIN, GPIO_AF2_TIM3);
    init_pwm_pin(RGB_B_PORT, RGB_B_PIN, GPIO_AF2_TIM3);

    /* TIM2 CH2 -> PB3 (Red) */
    TIM2->CR1 = 0;
    TIM2->PSC = 179u;
    TIM2->ARR = PWM_ARR;
    TIM2->CCR2 = 0;
    TIM2->CCMR1 = TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    TIM2->CCER = TIM_CCER_CC2E;
    TIM2->CR1 = TIM_CR1_ARPE;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->CR1 |= TIM_CR1_CEN;

    /* TIM3 CH1 -> PB4 (Green), CH2 -> PB5 (Blue) */
    TIM3->CR1 = 0;
    TIM3->PSC = 179u;
    TIM3->ARR = PWM_ARR;
    TIM3->CCR1 = 0;
    TIM3->CCR2 = 0;
    TIM3->CCMR1 = TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE
                | TIM_CCMR1_OC2M_2 | TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2PE;
    TIM3->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM3->CR1 = TIM_CR1_ARPE;
    TIM3->EGR = TIM_EGR_UG;
    TIM3->CR1 |= TIM_CR1_CEN;

    /* Tu kiem tra luc khoi dong: do -> xanh la -> xanh duong, moi mau 0.4 s. */
    RGB_Set(RGB_RED);   HAL_Delay(400);
    RGB_Set(RGB_GREEN); HAL_Delay(400);
    RGB_Set(RGB_BLUE);  HAL_Delay(400);
    RGB_Set(RGB_OFF);
}

void RGB_SetRGB(uint8_t r, uint8_t g, uint8_t b)
{
    TIM2->CCR2 = r;
    TIM3->CCR1 = g;
    TIM3->CCR2 = b;
}

void RGB_Set(rgb_color_t c)
{
    switch (c) {
    case RGB_RED:     RGB_SetRGB(255u,   0u,   0u); break;
    case RGB_GREEN:   RGB_SetRGB(  0u, 255u,   0u); break;
    case RGB_BLUE:    RGB_SetRGB(  0u,   0u, 255u); break;
    case RGB_YELLOW:  RGB_SetRGB(255u, 255u,   0u); break;
    case RGB_CYAN:    RGB_SetRGB(  0u, 255u, 255u); break;
    case RGB_MAGENTA: RGB_SetRGB(255u,   0u, 255u); break;
    case RGB_WHITE:   RGB_SetRGB(255u, 255u, 255u); break;
    case RGB_OFF:
    default:          RGB_SetRGB(  0u,   0u,   0u); break;
    }
}

