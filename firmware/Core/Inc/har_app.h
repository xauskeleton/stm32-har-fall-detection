/* Lop ung dung: noi driver cam bien voi thuat toan.
 *
 * KHONG phu thuoc HAL nen chay va test duoc tren PC. Hai viec nó lam la hai quyet
 * dinh PHAN CUNG, khong phai thuat toan:
 *
 *   1. HOAN VI TRUC  - board gan khac dien thoai cua HAPT
 *   2. KEP TIN HIEU  - HAPT bi bao hoa o +-2 g / +-10 rad/s nen model chua tung
 *                      thay gia tri vuot qua (xem docs/DECISIONS.md muc 8)
 *
 * Cach dung tu ngat TIM6 50 Hz:
 *
 *     ICM20948_ReadAccelGyro(&raw);
 *     float acc[3]  = { raw.ax / 8192.0f,  raw.ay / 8192.0f,  raw.az / 8192.0f };
 *     float gyro[3] = { raw.gx / 32.768f * DEG2RAD, ... };      // +-4 g / +-1000 dps
 *
 *     har_app_event_t ev;
 *     if (har_app_on_sample(acc, gyro, &ev)) {
 *         printf("%s\n", har_label_name(ev.label));
 *         if (ev.fall) { bat_canh_bao(); }
 *     }
 *
 * Don vi BAT BUOC: acc tinh bang g, gyro tinh bang rad/s - phai khop HAPT.
 */
#ifndef HAR_APP_H
#define HAR_APP_H

#include "fall_detect.h"
#include "model_params.h"

/* Gioi han kep - dung 2 moc ma cam bien cua HAPT bi bao hoa, ap cho TUNG TRUC.
 *
 * Kep tung truc KHONG lam chet nhanh phat hien soc: nguong FALL_IMPACT_G = 2.2 g do
 * tren CHUAN |a| = sqrt(x^2+y^2+z^2), ma chuan co the len toi 2*sqrt(3) = 3.46 g du
 * moi truc bi kep o 2 g. Du lieu HAPT cung bi bao hoa tung truc o 2 g va van co
 * |a| toi 2.43 g o phan vi 99 khi di lai - nen kep tung truc la DUNG NHU LUC TRAIN. */
#define HAR_CLAMP_ACC_G     2.0f
#define HAR_CLAMP_GYRO_RADS 10.0f

/* Hoan vi truc: board -> he truc cua HAPT.
 * Do tu du lieu ghi thuc te: dung yen thi gravity nam tren -X cua board, con HAPT
 * can +X  =>  (x, y, z)_hapt = (-x, +z, +y)_board, ap cho CA acc va gyro.
 * PHAI xac nhan board con gan dung nhu luc ghi truoc khi tin bo so nay.
 * Dat -DHAR_AXIS_PERMUTE=0 de tat (du lieu HAPT da o dung he truc nay).
 */
#ifndef HAR_AXIS_PERMUTE
#define HAR_AXIS_PERMUTE 1
#endif

typedef struct {
    int   label;      /* 1..HAR_N_CLASSES */
    int   fall;       /* 1 = vua phat hien te nga */
    int   recovered;  /* 1 = vua roi khoi trang thai FALLEN */
    float a_min;      /* |a| nho nhat trong cua so (g) */
    float a_max;      /* |a| lon nhat trong cua so (g) */
} har_app_event_t;

/* Goi mot lan luc khoi dong, va moi lan mat dong bo lay mau. */
void har_app_reset(void);

/* Goi moi mau o dung 50 Hz. Tra ve 1 khi co ket qua moi (ghi vao *ev), 0 khi chua
 * du mau (128 mau dau) hoac chua den luot phan loai (cu 64 mau mot lan).
 */
int har_app_on_sample(const float acc_g[3], const float gyro_rads[3],
                      har_app_event_t *ev);

/* Trang thai state machine hien tai - de hien LED hoac go loi. */
const fall_state_t *har_app_fall_state(void);

#endif /* HAR_APP_H */
