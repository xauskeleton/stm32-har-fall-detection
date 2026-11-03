#include "battery.h"
#include "stm32f4xx_hal.h"

// Pin LiPo 3S (11.1V): đầy = 12.6V, cạn = 10.5V
#define BAT_FULL_V      12.6f
#define BAT_EMPTY_V     10.5f
#define BAT_NONE_V      5.0f    // dưới mức này coi như không cắm pin
#define BAT_SHOW_MS     3000    // nhấn nút X1 thì đèn sáng trong 3 giây

static const uint16_t led_pins[5] = {
    GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5
};

static ADC_HandleTypeDef hadc1;
static float bat_v = 0.0f;
static uint8_t bat_pct = 0;

// Bật ADC1 để đọc chân PA5
void Battery_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC1_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_5;
    gpio.Mode = GPIO_MODE_ANALOG;
    HAL_GPIO_Init(GPIOA, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;   // 90 MHz / 4 (ADC chạy tối đa 36 MHz)
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;             // kết quả 0..4095
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = DISABLE;                      // chỉ đọc 1 kênh
    hadc1.Init.ContinuousConvMode = DISABLE;                // gọi lần nào đo lần đó
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    HAL_ADC_Init(&hadc1);

    ADC_ChannelConfTypeDef ch = {0};
    ch.Channel = ADC_CHANNEL_5;                             // PA5
    ch.Rank = 1;
    ch.SamplingTime = ADC_SAMPLETIME_480CYCLES;             // lấy mẫu chậm cho chính xác
    HAL_ADC_ConfigChannel(&hadc1, &ch);
}

// Đọc ADC 1 lần, trả về 0..4095
static uint16_t read_adc(void)
{
    HAL_ADC_Start(&hadc1);                    // bắt đầu đo
    HAL_ADC_PollForConversion(&hadc1, 10);    // chờ đo xong (tối đa 10 ms)
    return HAL_ADC_GetValue(&hadc1);          // lấy kết quả
}

// Đổi giá trị ADC ra điện áp pin
static float read_battery_voltage(void)
{
    float v_pin = read_adc() * 3.3f / 4095.0f;  // điện áp ở chân PA5 (0..3.3V)
    return v_pin * 10.0f + 0.35f;               // board chia áp pin cho 10, diode sụt 0.35V
}

// Đổi điện áp ra phần trăm (coi như tuyến tính từ cạn đến đầy)
static uint8_t voltage_to_percent(float v)
{
    if (v >= BAT_FULL_V)  return 100;
    if (v <= BAT_EMPTY_V) return 0;
    return (uint8_t)((v - BAT_EMPTY_V) / (BAT_FULL_V - BAT_EMPTY_V) * 100.0f);
}

// Bật n đèn đầu tiên, tắt các đèn còn lại (LED sáng khi ghi RESET)
static void show_leds(uint8_t n)
{
    for (uint8_t i = 0; i < 5; i++) {
        HAL_GPIO_WritePin(GPIOC, led_pins[i], (i < n) ? GPIO_PIN_RESET : GPIO_PIN_SET);
    }
}

// Gọi liên tục trong vòng lặp chính.
// Cứ 0.5 giây đo pin 1 lần; chỉ bật đèn trong 3 giây sau khi nhấn nút X1.
void Battery_Update(void)
{
    static uint32_t last_time = 0;
    static uint32_t show_start = 0;
    static bool showing = false;

    // Nút X1 (KEY3, PC11): nhấn = 0. Vừa nhấn xuống thì bắt đầu hiện đèn 3 giây
    static uint8_t btn_prev = 1;
    uint8_t btn = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_11);
    if (btn == 0 && btn_prev == 1) {
        showing = true;
        show_start = HAL_GetTick();
        last_time = HAL_GetTick() - 500;   // đo và bật đèn ngay, không chờ
    }
    btn_prev = btn;

    // Hết 3 giây thì tắt đèn
    if (showing && HAL_GetTick() - show_start >= BAT_SHOW_MS) {
        showing = false;
        show_leds(0);
    }

    if (HAL_GetTick() - last_time < 500) {
        return;
    }
    last_time = HAL_GetTick();

    bat_v = read_battery_voltage();
    bat_pct = (bat_v < BAT_NONE_V) ? 0 : voltage_to_percent(bat_v);

    if (!showing || bat_v < BAT_NONE_V) {   // không nhấn nút hoặc không có pin: đèn tắt
        show_leds(0);
        return;
    }

    if (bat_pct <= 10) {             // sắp hết pin: LED1 nhấp nháy
        static bool led_on = false;
        led_on = !led_on;
        show_leds(led_on ? 1 : 0);
    } else {                         // mỗi đèn = 20%
        show_leds((bat_pct + 19) / 20);
    }
}

uint32_t Battery_GetMillivolts(void)
{
    return (uint32_t)(bat_v * 1000.0f);
}

uint8_t Battery_GetPercent(void)
{
    return bat_pct;
}

bool Battery_IsPresent(void)
{
    return bat_v >= BAT_NONE_V;
}
