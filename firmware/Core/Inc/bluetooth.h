/* HC-05 qua UART4 (DMA1 Stream 4), LUON BAT. Gui len dien thoai:
 *
 *   moi BT_SEND_MS:   "Walking | Normal | |a|0.98..1.03 | g(+1.00,+0.00,-0.02) | bat 7.74V (85%)"
 *   ngay khi te nga:  "!!! FALL DETECTED (|a|0.48..2.65g) !!!"
 *   ngay khi dung day: "Recovered"
 *
 * Truyen khong chan bang DMA1 Stream 4 + hang doi vong 512 byte.
 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int          label;
    int          fall_state;
    float        a_min;
    float        a_max;
    const float *acc_raw;     /* float[3] (g), truoc hoan vi */
    const float *gyro_dps;    /* float[3] (deg/s), truoc hoan vi */
    const float *grav;        /* float[3] (g), sau hoan vi */
    int          active_run;
    int          guard;
    int          laying;
    uint32_t     missed;
    float        bat_v;
    int          bat_pct;     /* 0..100, hoac -1 neu khong cam pin */
    uint32_t     bat_adc;
    float        bat_pa5_v;
} bt_telemetry_t;

void Bluetooth_Init(void);

/* Goi lien tuc trong vong lap chinh: gui trang thai + tham so debug qua UART4 (DMA). */
void Bluetooth_Update(const bt_telemetry_t *t);

/* Dua 1 chuoi vao hang doi DMA; bo qua neu HC-05 chua khoi dong xong */
void Bluetooth_Send(const char *s);

/* Goi tu DMA1_Stream4_IRQHandler trong stm32f4xx_it.c */
void Bluetooth_DMA_IRQHandler(void);

#endif /* BLUETOOTH_H */
