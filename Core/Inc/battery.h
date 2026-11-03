#ifndef BATTERY_H
#define BATTERY_H

#include <stdint.h>
#include <stdbool.h>

// Đo pin LiPo 3S ở chân PA5, hiển thị % bằng LED1–LED5 (PC1–PC5):
// mỗi đèn = 20%, 5 đèn = đầy, LED1 nhấp nháy = còn dưới 10%.
// Đèn chỉ sáng 3 giây sau khi nhấn nút X1 (KEY3, PC11), bình thường tắt hết.
void Battery_Init(void);
void Battery_Update(void);         // gọi liên tục trong vòng lặp chính
uint32_t Battery_GetMillivolts(void);
uint8_t Battery_GetPercent(void);
bool Battery_IsPresent(void);      // false khi chỉ cắm USB, không có pin

#endif /* BATTERY_H */
