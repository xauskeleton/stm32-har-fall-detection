/* Duong ong streaming: nap tung mau o 50 Hz, cu HAR_STEP mau moi thi phan loai 1 lan.
 *
 * Giu trang thai bo loc qua cac mau nen KHONG co transient giua cac cua so, giong
 * har/dsp.py loc tren toan bo chuoi lien tuc truoc khi cat cua so.
 *
 * Bo nho: buffer tin hieu 5 nhom x 3 truc x 128 float = 7680 byte, cong kenh soc
 * 128 x 3 float = 1536 byte, cong scratch cua har_dsp/har_features ~2 KB.
 */
#ifndef HAR_PIPELINE_H
#define HAR_PIPELINE_H

#include "model_params.h"

typedef struct {
    int   label;     /* 1..HAR_N_CLASSES */
    float a_min;     /* |a| nho nhat trong cua so (g), cho fall_update() */
    float a_max;     /* |a| lon nhat trong cua so (g) */
} har_result_t;

/* Goi mot lan luc khoi dong, va moi lan mat dong bo lay mau. */
void har_pipeline_reset(void);

/* Goi moi mau o dung 50 Hz. acc don vi g, gyro don vi rad/s - PHAI khop HAPT.
 * Tra ve 1 khi co ket qua moi (ghi vao *out), 0 khi chua du mau.
 *
 * Phan cung doc thang ra gia tri vuot +-2 g / +-10 rad/s, con model chua tung thay
 * gia tri do (HAPT bi bao hoa o dung 2 moc nay) -> PHAI kep acc vao +-2.0 g va gyro
 * vao +-10 rad/s TRUOC khi goi ham nay. Xem docs/DECISIONS.md muc 8.
 */
int har_pipeline_push(const float acc[3], const float gyro[3], har_result_t *out);

/* Vector HAR_N_FEATURES_ALL dac trung cua lan phan loai gan nhat - de doi chieu voi
 * golden_vectors.h va de go loi tren board. Chi hop le sau khi push() tra ve 1. */
const float *har_pipeline_features(void);

#endif /* HAR_PIPELINE_H */
