#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>

// Còi chủ động gắn ngoài ở PA8 (chân A8 hàng header trên): chân + vào A8, chân − vào G.
// Chỉ kêu khi phát hiện té ngã, theo nhịp "tít tít tèèè — nghỉ" lặp lại.
void Buzzer_Init(void);
void Buzzer_Start(void);
void Buzzer_Stop(void);
void Buzzer_Update(void);   // gọi liên tục trong vòng lặp chính
bool Buzzer_IsOn(void);

#endif /* BUZZER_H */
