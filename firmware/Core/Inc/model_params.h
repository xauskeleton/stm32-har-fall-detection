/* SINH TU DONG boi scripts/export_model.py ngay 2026-09-22 - KHONG SUA TAY.
 *
 * Model : logreg 120-12, accuracy 86.88% tren tap test 9 nguoi chua tung thay
 * Nhom  : total_acc, body_acc, gyro
 */
#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

/* ---- Lay mau va cat cua so ---- */
#define HAR_FS               50     /* Hz */
#define HAR_WINDOW           128    /* mau = 2.56 s */
#define HAR_STEP             64   /* mau giua 2 lan phan loai = 1.28 s */
#define HAR_N_SPECTRUM       64   /* so bin pho = HAR_WINDOW/2 */

/* ---- Kich thuoc ---- */
#define HAR_N_GROUPS         3
#define HAR_N_CHANNELS       4    /* x, y, z, magnitude */
#define HAR_N_FEATURES       120
#define HAR_N_CLASSES        12

/* ---- Thu tu nhom tin hieu: QUYET DINH thu tu dac trung, dao la model sai im lang ---- */
#define HAR_GROUP_TOTAL_ACC  0
#define HAR_GROUP_BODY_ACC   1
#define HAR_GROUP_GYRO       2

/* ---- Loc ---- */
#define HAR_N_SOS            2   /* section biquad cho lowpass 0.3 Hz tach gravity */
#define HAR_MEDIAN_SHOCK     3    /* cua so median, CHI cho kenh phat hien soc */

/* ---- Nguong phat hien te nga ---- */
#define FALL_FREEFALL_G      0.600000024f
#define FALL_IMPACT_G        2.0f
#define FALL_CONFIRM_WIN     4
#define FALL_LAYING_CONFIRM  2
#define FALL_VOLUNTARY_GUARD 4
#define FALL_RECOVER_WIN     2

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
extern const float HAR_SOS_GRAVITY[HAR_N_SOS][6];

/* Chuan hoa: x_chuan[i] = (x[i] - HAR_SCALER_MEAN[i]) * HAR_SCALER_INV_SCALE[i] */
extern const float HAR_SCALER_MEAN[HAR_N_FEATURES];
extern const float HAR_SCALER_INV_SCALE[HAR_N_FEATURES];

/* Phan loai: score[k] = HAR_BIAS[k] + sum_j HAR_WEIGHTS[k][j] * x_chuan[j], lay argmax */
extern const float HAR_WEIGHTS[HAR_N_CLASSES][HAR_N_FEATURES];
extern const float HAR_BIAS[HAR_N_CLASSES];

/* Ten nhan de in ra UART. label 1..HAR_N_CLASSES, ngoai khoang tra ve "?" */
const char *har_label_name(int label);

#endif /* MODEL_PARAMS_H */
