/* Coi chu dong (cap dien la keu) - chi keu khi te nga, nhip "tit tit teee - nghi".
 * Khong chan: Buzzer_Update() doi buoc theo HAL_GetTick().
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdbool.h>

void Buzzer_Init(void);
void Buzzer_Start(void);
void Buzzer_Stop(void);
void Buzzer_Update(void);   /* goi lien tuc trong vong lap chinh */
bool Buzzer_IsOn(void);

#endif /* BUZZER_H */
