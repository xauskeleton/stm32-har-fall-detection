#include "battery.h"
#include "board_config.h"
#include "rgb_led.h"

/* Doc ADC1 bang thanh ghi truc tiep (khong can driver HAL ADC trong project). */

#define ADC_N_AVG     16u
#define ADC_TIMEOUT   20000u    /* vong cho EOC; 1 lan chuyen doi ~22 us */
#define V_EMA_ALPHA   0.3f      /* lam muot dien ap giua cac lan do */

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

/* Mau LED RGB theo % pin (bang mau trong board_config.h). Binh thuong tat de
 * tiet kiem pin; sap het pin thi nhay do ke ca khi khong nhan nut. */
static void refresh_leds(void)
{
    uint32_t now = HAL_GetTick();
    bool fast_on = ((now / RGB_BLINK_MS) & 1u) == 0u;
    bool slow_on = ((now / BAT_BLINK_SLOW_MS) & 1u) == 0u;
    bool present = bat_v >= BAT_NONE_V;

    if (present && bat_v < BAT_EMPTY_V) {
        RGB_Set(fast_on ? RGB_RED : RGB_OFF);       /* sap het pin: bao ca khi khong nhan nut */
    } else if (!showing) {
        RGB_Set(RGB_OFF);
    } else if (!present) {
        RGB_Set(RGB_BLUE);                          /* chi cap nguon USB */
    } else if (bat_pct > BAT_PCT_FULL) {
        RGB_Set(RGB_GREEN);
    } else if (bat_pct > BAT_PCT_GOOD) {
        RGB_Set(slow_on ? RGB_GREEN : RGB_OFF);
    } else if (bat_pct > BAT_PCT_HALF) {
        RGB_Set(RGB_YELLOW);
    } else if (bat_pct > BAT_PCT_LOW) {
        RGB_Set(slow_on ? RGB_YELLOW : RGB_OFF);
    } else {
        RGB_Set(RGB_RED);
    }
}

void Battery_Init(void)
{
    RGB_Init();
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
