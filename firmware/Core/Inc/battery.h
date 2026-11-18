/* Do pin 2S qua PA5 va hien % bang 5 LED tren board (moi LED = 20%).
 * Binh thuong 5 LED tat de tiet kiem pin; goi Battery_Show() thi sang 3 s.
 * Duoi 10% thi LED1 nhap nhay. Khong cam pin (chi USB) thi LED tat.
 */
#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

void    Battery_Init(void);
void    Battery_Update(void);       /* goi lien tuc trong vong lap chinh */
void    Battery_Show(void);         /* sang LED bao pin trong BAT_SHOW_MS */
float   Battery_GetVoltage(void);   /* V, da cong sut ap diode */
uint8_t Battery_GetPercent(void);   /* 0..100 */
bool    Battery_IsPresent(void);

#endif /* BATTERY_H */
