/* HC-05 qua UART4 (DMA1 Stream 4), LUON BAT. Kenh log DUY NHAT cua thiet bi:
 * moi cua so phan loai (64 mau = 1.28 s) gui 1 dong
 *
 *   "Walking | SHOCK | amin=0.55 amax=2.13 | r=0 g=4 l=0 t=4 s=1 u=0"
 *
 *   hanh dong (len/xuong cau thang hien chung "Walking") | IDLE/SHOCK/FALLEN |
 *   amin / amax = |a| nho nhat / lon nhat (g) cua cua so |
 *   r = so cua so dung/di lien tiep, g = guard nam chu dong, l = so lan LAYING trong SHOCK,
 *   t = so cua so con lai de xac nhan, s = vao SHOCK bang soc that, u = so cua so dung/di
 *   lien tiep tu luc vao SHOCK.
 *
 * Truyen khong chan bang DMA1 Stream 4 + hang doi vong 512 byte.
 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>
#include "fall_detect.h"

void Bluetooth_Init(void);

/* Goi 1 lan moi cua so phan loai (khi har_app_on_sample() tra ve 1). */
void Bluetooth_LogWindow(int label, const fall_state_t *fs, float a_min, float a_max);

/* Dua 1 chuoi vao hang doi DMA; bo qua neu HC-05 chua khoi dong xong */
void Bluetooth_Send(const char *s);

/* Goi tu DMA1_Stream4_IRQHandler trong stm32f4xx_it.c */
void Bluetooth_DMA_IRQHandler(void);

#endif /* BLUETOOTH_H */
