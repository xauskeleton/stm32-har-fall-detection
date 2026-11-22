#include "rgb_led.h"
#include "board_config.h"

/* Cuc am chung: ghi 1 la sang */
static void write_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    HAL_GPIO_WritePin(RGB_R_PORT, RGB_R_PIN, r ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_G_PORT, RGB_G_PIN, g ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(RGB_B_PORT, RGB_B_PIN, b ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static void init_pin(GPIO_TypeDef *port, uint16_t pin)
{
    HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = pin;
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(port, &gpio);
}

void RGB_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    /* PB3 (SWO) va PB4 (NJTRST) mac dinh la chan JTAG. Nap/debug SWD chi can
     * PA13/PA14 nen doi sang GPIO van binh thuong; chi mat SWV trace qua SWO,
     * ma du an in log qua USART1 nen khong dung SWV. */
    init_pin(RGB_R_PORT, RGB_R_PIN);
    init_pin(RGB_G_PORT, RGB_G_PIN);
    init_pin(RGB_B_PORT, RGB_B_PIN);

    /* Tu kiem tra luc khoi dong: do -> xanh la -> xanh duong, moi mau 0.4 s.
     * Mau nao khong sang hoac sang sai mau = day / chan cua mau do co van de. */
    RGB_Set(RGB_RED);   HAL_Delay(400);
    RGB_Set(RGB_GREEN); HAL_Delay(400);
    RGB_Set(RGB_BLUE);  HAL_Delay(400);
    RGB_Set(RGB_OFF);
}

void RGB_Set(rgb_color_t c)
{
    switch (c) {
    case RGB_RED:     write_rgb(1, 0, 0); break;
    case RGB_GREEN:   write_rgb(0, 1, 0); break;
    case RGB_BLUE:    write_rgb(0, 0, 1); break;
    case RGB_YELLOW:  write_rgb(1, 1, 0); break;
    case RGB_CYAN:    write_rgb(0, 1, 1); break;
    case RGB_MAGENTA: write_rgb(1, 0, 1); break;
    case RGB_WHITE:   write_rgb(1, 1, 1); break;
    case RGB_OFF:
    default:          write_rgb(0, 0, 0); break;
    }
}

