#include "battery.h"
#include "board_config.h"
#include "rgb_led.h"

/* Doc ADC1 bang thanh ghi truc tiep (khong can driver HAL ADC trong project). */

#define ADC_N_AVG     16u
#define ADC_TIMEOUT   20000u    /* vong cho EOC; 1 lan chuyen doi ~22 us */
#define V_EMA_ALPHA   0.3f      /* lam muot dien ap giua cac lan do */

volatile uint32_t g_bat_adc_raw = 0;
volatile float    g_bat_pa5_v   = 0.0f;
volatile float    g_bat_v       = 0.0f;
volatile uint8_t  g_bat_pct     = 0;

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
    float v_pa5 = (float)raw * 3.3f / 4095.0f;
    float v = v_pa5 * BAT_DIVIDER;
    if (v > 1.0f) v += BAT_DIODE_V;          /* co dien ap thi moi co sut ap diode */

    /* Lan dau, hoac vua cam / rut pin: lay luon gia tri moi, khong lam muot */
    if (bat_v < BAT_NONE_V || v < BAT_NONE_V) bat_v = v;
    else bat_v += V_EMA_ALPHA * (v - bat_v);

    bat_pct = (bat_v < BAT_NONE_V) ? 0u : voltage_to_percent(bat_v);

    g_bat_adc_raw = raw;
    g_bat_pa5_v   = v_pa5;
    g_bat_v       = bat_v;
    g_bat_pct     = bat_pct;
}

/* Bang 10 muc mau PWM sang lien tuc theo % pin (moi nac 10% = 0.18 V).
 * Kenh Do giam nhe khi pha mau vi LED do co Vf thap (~2.0 V) sang manh hon G/B. */
static const struct {
    uint8_t gt_pct;
    uint8_t r, g, b;
} BAT_LEVELS[] = {
    { 90u, 140u, 255u, 220u },   /* Trang */
    { 80u,   0u, 255u, 200u },   /* Xanh ngoc (Cyan) */
    { 70u,   0u, 255u,  45u },   /* Xanh luc bao */
    { 60u,   0u, 255u,   0u },   /* Xanh la */
    { 50u,  60u, 255u,   0u },   /* Xanh non chuoi */
    { 40u, 150u, 255u,   0u },   /* Vang */
    { 30u, 220u, 120u,   0u },   /* Cam vang */
    { 20u, 255u,  45u,   0u },   /* Cam dam */
    { 10u, 200u,   0u, 160u },   /* Hong tim */
    {  0u, 255u,   0u,   0u },   /* Do */
};

/* Mau LED RGB theo % pin. Binh thuong tat de tiet kiem pin;
 * duoi BAT_EMPTY_V thi nhay do nhanh ke ca khi khong nhan nut. */
static void refresh_leds(void)
{
    uint32_t now = HAL_GetTick();
    bool fast_on = ((now / RGB_BLINK_MS) & 1u) == 0u;
    bool present = bat_v >= BAT_NONE_V;

    if (present && bat_v < BAT_EMPTY_V) {
        RGB_Set(fast_on ? RGB_RED : RGB_OFF);
        return;
    }
    if (!showing) {
        RGB_Set(RGB_OFF);
        return;
    }
    if (!present) {
        RGB_Set(RGB_BLUE);
        return;
    }
    for (uint32_t i = 0; i < sizeof(BAT_LEVELS) / sizeof(BAT_LEVELS[0]); i++) {
        if (bat_pct > BAT_LEVELS[i].gt_pct || BAT_LEVELS[i].gt_pct == 0u) {
            RGB_SetRGB(BAT_LEVELS[i].r, BAT_LEVELS[i].g, BAT_LEVELS[i].b);
            return;
        }
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
