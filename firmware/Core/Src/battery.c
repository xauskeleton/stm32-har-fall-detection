#include "battery.h"
#include "board_config.h"

/* Doc ADC1 bang thanh ghi truc tiep (khong can driver HAL ADC trong project). */

#define ADC_N_AVG     16u
#define ADC_TIMEOUT   20000u    /* vong cho EOC; 1 lan chuyen doi ~22 us */
#define V_EMA_ALPHA   0.3f      /* lam muot dien ap giua cac lan do */

static const uint16_t led_pins[5] = BAT_LED_PINS;

static float   bat_v = 0.0f;
static uint8_t bat_pct = 0;
static bool    showing = false;
static uint32_t show_start = 0;
static uint32_t last_measure = 0;

static void adc_init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = BAT_ADC_PIN;
    gpio.Mode = GPIO_MODE_ANALOG;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(BAT_ADC_PORT, &gpio);

    /* PCLK2 90 MHz / 4 = 22.5 MHz (ADC toi da 36 MHz) */
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    ADC1->CR1 = 0;                          /* 12 bit, khong scan */
    ADC1->CR2 = 0;                          /* can phai, chuyen doi tung lan */
    ADC1->SQR1 = 0;                         /* 1 lan chuyen doi trong chuoi */
    ADC1->SQR3 = BAT_ADC_CHANNEL;
    /* 480 chu ky lay mau: nguon la RC 470R + 470nF nen lay mau cham cho chac */
    ADC1->SMPR2 = (ADC1->SMPR2 & ~(7u << (3u * BAT_ADC_CHANNEL)))
                | (7u << (3u * BAT_ADC_CHANNEL));
    ADC1->CR2 |= ADC_CR2_ADON;
    for (volatile uint32_t i = 0; i < 1000u; i++) { }   /* cho ADC on dinh (~3 us) */
}

static int adc_read_avg(uint32_t *out)
{
    uint32_t sum = 0;
    for (uint32_t k = 0; k < ADC_N_AVG; k++) {
        ADC1->SR = 0;
        ADC1->CR2 |= ADC_CR2_SWSTART;
        uint32_t t = 0;
        while (!(ADC1->SR & ADC_SR_EOC)) {
            if (++t > ADC_TIMEOUT) return 0;
        }
        sum += ADC1->DR & 0x0FFFu;
    }
    *out = sum / ADC_N_AVG;
    return 1;
}

static uint8_t voltage_to_percent(float v)
{
    if (v >= BAT_FULL_V)  return 100u;
    if (v <= BAT_EMPTY_V) return 0u;
    return (uint8_t)((v - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V) * 100.0f);
}

/* Bat n LED dau, tat cac LED con lai (keo LOW la sang) */
static void show_leds(uint8_t n)
{
    for (uint8_t i = 0; i < 5u; i++) {
        HAL_GPIO_WritePin(BAT_LED_PORT, led_pins[i], (i < n) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

static void measure(void)
{
    uint32_t raw;
    if (!adc_read_avg(&raw)) return;
    float v = (float)raw * 3.3f / 4095.0f * BAT_DIVIDER;
    if (v > 1.0f) v += BAT_DIODE_V;          /* co dien ap thi moi co sut ap diode */

    /* Lan dau, hoac vua cam / rut pin: lay luon gia tri moi, khong lam muot */
    if (bat_v < BAT_NONE_V || v < BAT_NONE_V) bat_v = v;
    else bat_v += V_EMA_ALPHA * (v - bat_v);

    bat_pct = (bat_v < BAT_NONE_V) ? 0u : voltage_to_percent(bat_v);
}

static void refresh_leds(void)
{
    if (!showing || bat_v < BAT_NONE_V) {
        show_leds(0);
        return;
    }
    if (bat_pct <= BAT_LOW_PCT) {
        /* sap het pin: LED1 nhap nhay 2 Hz */
        show_leds(((HAL_GetTick() / 250u) & 1u) ? 1u : 0u);
    } else {
        show_leds((uint8_t)((bat_pct + 19u) / 20u));   /* 1..5 LED */
    }
}

void Battery_Init(void)
{
    __HAL_RCC_GPIOC_CLK_ENABLE();
    GPIO_InitTypeDef gpio = {0};
    for (uint8_t i = 0; i < 5u; i++) gpio.Pin |= led_pins[i];
    gpio.Mode = GPIO_MODE_OUTPUT_PP;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_WritePin(BAT_LED_PORT, gpio.Pin, GPIO_PIN_SET);   /* tat truoc khi bat output */
    HAL_GPIO_Init(BAT_LED_PORT, &gpio);

    adc_init();
    measure();
    last_measure = HAL_GetTick();
}

void Battery_Update(void)
{
    uint32_t now = HAL_GetTick();

    if (now - last_measure >= BAT_MEASURE_MS) {
        last_measure = now;
        measure();
    }
    if (showing && now - show_start >= BAT_SHOW_MS) {
        showing = false;
    }
    refresh_leds();
}

void Battery_Show(void)
{
    measure();                 /* do ngay de hien so moi nhat */
    showing = true;
    show_start = HAL_GetTick();
    refresh_leds();
}

float Battery_GetVoltage(void)  { return bat_v; }
uint8_t Battery_GetPercent(void) { return bat_pct; }
bool Battery_IsPresent(void)    { return bat_v >= BAT_NONE_V; }
