/* Do pin 2S qua PA5 va bao muc pin bang mau LED RGB (bang mau trong board_config.h).
 * Binh thuong LED tat de tiet kiem pin; goi Battery_Show() thi sang 3 s.
 * Duoi BAT_EMPTY_V (4.0 V) thi nhay do lien tuc, khong can nhan nut.
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
