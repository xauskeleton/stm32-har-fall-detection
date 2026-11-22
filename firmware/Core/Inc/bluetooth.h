/* HC-05 qua UART4, LUON BAT (PB1 = 1 cap nguon). Gui len dien thoai:
 *
 *   moi BT_SEND_MS:   "Di bo | Binh thuong | pin 76%"
 *   ngay khi te nga:  "!!! TE NGA !!!"
 *   ngay khi dung day: "Da dung day"
 *
 * Gui khong chan: moi lan goi Bluetooth_Update() day toi da 1 byte, nen khong
 * lam lo nhip lay mau 50 Hz. Dien thoai doc bang app Serial Bluetooth Terminal.
 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

void Bluetooth_Init(void);

/* Goi lien tuc trong vong lap chinh.
 *   label       = hanh dong 1..12 (0 = chua co ket qua)
 *   fall_state  = FALL_STATE_*
 *   battery_pct = 0..100, hoac -1 neu khong cam pin */
void Bluetooth_Update(int label, int fall_state, int battery_pct);

/* Dua 1 chuoi vao hang doi gui; bo qua neu HC-05 chua khoi dong xong */
void Bluetooth_Send(const char *s);

#endif /* BLUETOOTH_H */
