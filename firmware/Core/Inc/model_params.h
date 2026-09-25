/* SINH TU DONG boi scripts/export_model.py ngay 2026-09-25 - KHONG SUA TAY.
 *
 * Model : logreg 160-12, accuracy 93.23% tren tap test 9 nguoi chua tung thay
 * Nhom  : body_acc, gravity, body_acc_jerk, gyro, gyro_jerk
 */
#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>

/* ---- Lay mau va cat cua so ---- */
#define HAR_FS               50     /* Hz */
#define HAR_WINDOW           128    /* mau = 2.56 s */
#define HAR_STEP             64   /* mau giua 2 lan phan loai = 1.28 s */

/* ---- Kich thuoc ---- */
#define HAR_N_SIGNALS        5   /* nhom tin hieu 3 truc */
#define HAR_N_FEATURES_ALL   516   /* so dac trung TINH RA */
#define HAR_N_FEATURES       160  /* so dac trung GIU LAI -> vao model */
#define HAR_N_CLASSES        12

/* ---- Thu tu nhom tin hieu: QUYET DINH thu tu dac trung, dao la model sai im lang ---- */
#define HAR_SIG_BODY_ACC         0
#define HAR_SIG_GRAVITY          1
#define HAR_SIG_BODY_ACC_JERK    2
#define HAR_SIG_GYRO             3
#define HAR_SIG_GYRO_JERK        4

/* ---- Mien tan so ---- */
#define HAR_N_SPECTRUM       64  /* bin 1..64 cua FFT 128 diem, BO bin DC */
#define HAR_SPEC_SCALE       64.0f /* chia pho cho so nay -> bien do 1 phia */
#define HAR_N_BANDS          14  /* so dai tan cho bandsEnergy */

/* ---- Cac phep dac biet ---- */
#define HAR_ENTROPY_BINS     256  /* histogram o CO DINH tren [0,1] */
#define HAR_AR_ORDER         4  /* bac mo hinh AR (Burg) */

/* ---- Loc: median 3 -> lowpass 20 Hz -> tach gravity 0.3 Hz ---- */
#define HAR_MEDIAN           3    /* cua so median, cho CA dac trung VA kenh soc */
#define HAR_N_SOS            2   /* section biquad, giong nhau cho ca 2 bo loc */

/* ---- Nguong phat hien te nga ---- */
#define FALL_FREEFALL_G      0.600000024f
#define FALL_IMPACT_G        2.20000005f
#define FALL_CONFIRM_WIN     4
#define FALL_LAYING_CONFIRM  2
#define FALL_VOLUNTARY_GUARD 4
#define FALL_RECOVER_WIN     2
#define FALL_UPRIGHT_MIN     4

/* ---- Nhan hoat dong (giu nguyen ID cua HAPT) ---- */
#define HAR_WALKING               1
#define HAR_WALKING_UPSTAIRS      2
#define HAR_WALKING_DOWNSTAIRS    3
#define HAR_SITTING               4
#define HAR_STANDING              5
#define HAR_LAYING                6
#define HAR_STAND_TO_SIT          7
#define HAR_SIT_TO_STAND          8
#define HAR_SIT_TO_LIE            9
#define HAR_LIE_TO_SIT           10
#define HAR_STAND_TO_LIE         11
#define HAR_LIE_TO_STAND         12

/* ---- So lieu (dinh nghia trong model_params.c) ---- */

/* SOS: [section][6] = b0, b1, b2, a0, a1, a2  (a0 luon = 1) */
extern const float HAR_SOS_NOISE[HAR_N_SOS][6];      /* lowpass 20 Hz khu nhieu */
extern const float HAR_SOS_GRAVITY[HAR_N_SOS][6];    /* lowpass 0.3 Hz tach gravity */

/* Trang thai XAC LAP cho dau vao = 1.0. Khoi tao state = ZI[section][i] * mau_dau_tien,
 * nho do khong co transient. Bo qua thi 4 cua so dau sau moi lan cap dien sai nhan. */
extern const float HAR_SOS_NOISE_ZI[HAR_N_SOS][2];
extern const float HAR_SOS_GRAVITY_ZI[HAR_N_SOS][2];

/* Bien cac dai tan cho bandsEnergy: [dai][2] = bin dau, bin cuoi (dem tu 1) */
extern const uint8_t HAR_BANDS[HAR_N_BANDS][2];

/* Chon cot: x_chon[i] = dac_trung[HAR_SELECTED[i]], i = 0..HAR_N_FEATURES-1 */
extern const uint16_t HAR_SELECTED[HAR_N_FEATURES];

/* Chuan hoa: x_chuan[i] = (x_chon[i] - HAR_SCALER_MEAN[i]) * HAR_SCALER_INV_SCALE[i] */
extern const float HAR_SCALER_MEAN[HAR_N_FEATURES];
extern const float HAR_SCALER_INV_SCALE[HAR_N_FEATURES];

/* Phan loai: score[k] = HAR_BIAS[k] + sum_j HAR_WEIGHTS[k][j] * x_chuan[j], lay argmax */
extern const float HAR_WEIGHTS[HAR_N_CLASSES][HAR_N_FEATURES];
extern const float HAR_BIAS[HAR_N_CLASSES];

/* Ten nhan de in ra UART. label 1..HAR_N_CLASSES, ngoai khoang tra ve "?" */
const char *har_label_name(int label);

#endif /* MODEL_PARAMS_H */
