/* Do pin 2S qua PA5 va bao muc pin bang mau LED RGB (bang mau trong board_config.h).
 * Binh thuong LED tat de tiet kiem pin; goi Battery_Show() thi sang 3 s.
 * Duoi BAT_EMPTY_V (6.2 V) thi nhay do lien tuc, khong can nhan nut.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

/* Bien toan cuc de xem nhanh trong Live Expressions (STM32CubeIDE) */
extern volatile uint32_t g_bat_adc_raw;   /* gia tri ADC trung binh 0..4095 tai PA5 */
extern volatile float    g_bat_pa5_v;     /* dien ap tai chan PA5 (V) */
extern volatile float    g_bat_v;         /* dien ap pin sau quy doi + bu diode (V) */
extern volatile uint8_t  g_bat_pct;       /* phan tram pin 0..100 */

void    Battery_Init(void);
void    Battery_Update(void);       /* goi lien tuc trong vong lap chinh */
void    Battery_Show(void);         /* sang LED bao pin trong BAT_SHOW_MS */
float   Battery_GetVoltage(void);   /* V, da cong sut ap diode */
uint8_t Battery_GetPercent(void);   /* 0..100 */
bool    Battery_IsPresent(void);

#endif /* BATTERY_H */
