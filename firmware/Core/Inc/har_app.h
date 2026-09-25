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

/* ==========================================================================
 * HOAN VI TRUC - PHAI HIEU CHUAN CHO TUNG CACH GAN BOARD
 * ==========================================================================
 *
 * HAPT thu bang dien thoai kep o THAT LUNG, va huong gravity quyet dinh tu the:
 *
 *     DUNG / NGOI / DI BO :  gravity ~ (+1.00, -0.17, -0.02) g  -> nam tren +X
 *     NAM                 :  gravity ~ (+0.07, +0.64, +0.54) g  -> X gan 0
 *
 * => Neu gravity KHONG nam tren +X luc dung yen, model doc thanh LAYING bat ke nguoi
 *    dang dung. Day la loi hay gap nhat khi dua len board moi.
 *
 * CACH HIEU CHUAN (khong can nap lai firmware nhieu lan):
 *
 *   1. Gan board DUNG NHU khi dung thuc te, roi DUNG THANG va giu yen.
 *   2. Doc dong UART: "acc tho" cho 3 gia tri cua board.
 *   3. Truc nao doc gan +-1.0 g chinh la truc chi xuong dat. Dat:
 *        HAR_AXIS_SRC_X  = so hieu truc do  (0 = x, 1 = y, 2 = z)
 *        HAR_AXIS_SIGN_X = -1 neu no doc -1.0 g, +1 neu doc +1.0 g
 *      Hai truc con lai dien vao Y va Z theo thu tu nao cung duoc.
 *   4. Nap lai, kiem dong "gravity" phai ra (+1.0, ~0, ~0).
 *
 * Vi du: board doc (-1.0, 0, 0) luc dung  ->  SRC_X = 0, SIGN_X = -1
 *        board doc (0, 0, +1.0) luc dung  ->  SRC_X = 2, SIGN_X = +1
 */
#define HAR_AXIS_SRC_X   0
#define HAR_AXIS_SIGN_X  (-1.0f)
#define HAR_AXIS_SRC_Y   2
#define HAR_AXIS_SIGN_Y  (+1.0f)
#define HAR_AXIS_SRC_Z   1
#define HAR_AXIS_SIGN_Z  (+1.0f)

/* Dat -DHAR_AXIS_PERMUTE=0 de tat hoan vi (du lieu HAPT da o dung he truc nay). */
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

/* Uoc luong gravity SAU hoan vi (loc thong thap 1 chieu), CHI de hieu chuan va go loi -
 * khong dung trong duong tinh dac trung. Dung yen thi phai ra (+1.0, ~0, ~0). */
void har_app_gravity(float out[3]);

#endif /* HAR_APP_H */
