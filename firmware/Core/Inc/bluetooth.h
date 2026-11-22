/* HC-05 qua USART2, nguon bat/tat bang PB1 (xem so do trong board_config.h).
 *
 * Tiet kiem pin: binh thuong HC-05 TAT. Bat khi:
 *   - te nga: giu bat suot luc FALLEN, them BT_HOLD_MS sau khi dung day
 *   - nhan nut xem pin: bat BT_HOLD_MS de dien thoai ket noi xem trang thai
 * Khi bat, moi BT_SEND_MS gui 1 dong trang thai. Gui khong chan: moi lan goi
 * Bluetooth_Update() day toi da 1 byte, nen khong lam lo nhip lay mau 50 Hz.
 */
#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include <stdint.h>
#include <stdbool.h>

void Bluetooth_Init(void);

/* Bat nguon (neu dang tat) va giu toi it nhat now + hold_ms */
void Bluetooth_Wake(uint32_t hold_ms);

/* Goi lien tuc trong vong lap chinh. fall_state = FALL_STATE_*,
 * battery_pct = 0..100 hoac -1 neu khong cam pin. */
void Bluetooth_Update(int fall_state, int battery_pct);

/* Dua 1 chuoi vao hang doi gui; bo qua neu HC-05 dang tat hoac chua khoi dong xong */
void Bluetooth_Send(const char *s);

bool Bluetooth_IsOn(void);

#endif /* BLUETOOTH_H */
