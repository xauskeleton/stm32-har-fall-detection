#include "buttons.h"
#include "board_config.h"

int Button_Pressed(button_t *b)
{
    uint8_t raw = (HAL_GPIO_ReadPin(b->port, b->pin) == GPIO_PIN_SET) ? 1u : 0u;
    uint32_t now = HAL_GetTick();

    if (raw != b->last_raw) {          /* muc vua doi -> bat dau dem chong rung */
        b->last_raw = raw;
        b->t_change = now;
        return 0;
    }
    if (raw != b->stable && now - b->t_change >= BTN_DEBOUNCE_MS) {
        b->stable = raw;
        return raw == 0u;              /* nut noi GND: 0 = vua nhan */
    }
    return 0;
}
