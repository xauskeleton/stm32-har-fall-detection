/* LED RGB cuc am chung bao trang thai:
 *   xanh la        = binh thuong (IDLE)
 *   vang           = vua co va dap, dang cho xac nhan (SHOCK)
 *   do nhap nhay   = da te nga (FALLEN)
 *   xanh duong nhay = loi cam bien (khong doc duoc ICM-20948)
 */
#ifndef RGB_LED_H
#define RGB_LED_H

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

/* Goi lien tuc trong vong lap chinh. fall_state = FALL_STATE_*, sensor_ok = 0 neu
 * IMU loi. Nhap nhay tinh theo HAL_GetTick() nen khong chan. */
void RGB_Update(int fall_state, int sensor_ok);

#endif /* RGB_LED_H */
