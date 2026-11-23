/* LED RGB cuc am chung, dung de bao pin (logic mau nam o battery.c). */
#ifndef RGB_LED_H
#define RGB_LED_H

#include <stdint.h>

typedef enum {
    RGB_OFF = 0,
    RGB_RED,
    RGB_GREEN,
    RGB_BLUE,
    RGB_YELLOW,
    RGB_CYAN,
    RGB_MAGENTA,
    RGB_WHITE
} rgb_color_t;

void RGB_Init(void);
void RGB_Set(rgb_color_t c);
void RGB_SetRGB(uint8_t r, uint8_t g, uint8_t b);

#endif /* RGB_LED_H */
