/* Doc nut co chong rung, phat hien canh nhan xuong. Doc trong vong lap chinh,
 * KHONG dung ngat EXTI - tranh chay code nang (do ADC, printf) trong ngat.
 */
#ifndef BUTTONS_H
#define BUTTONS_H

#include "stm32f4xx_hal.h"

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint8_t       stable;      /* muc da on dinh: 1 = nha, 0 = nhan */
    uint8_t       last_raw;
    uint32_t      t_change;
} button_t;

#define BUTTON_INIT(p, n)  { (p), (n), 1u, 1u, 0u }

/* Tra ve 1 dung mot lan moi khi nut vua duoc nhan xuong */
int Button_Pressed(button_t *b);

#endif /* BUTTONS_H */
