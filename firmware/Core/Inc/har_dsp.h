/* Khoi DSP: median 3, cascade biquad, FFT 128 diem -> pho 64 bin.
 *
 * Ban tham chieu Python: har/dsp.py. Da doi chieu tren 3162 cua so tap test:
 * 0 cua so lech nhan, sai so dac trung lon nhat 0.248 (xem docs/DECISIONS.md muc 7).
 *
 * Toan bo dung float, KHONG dung double: FPU cua Cortex-M4 chi co single precision.
 */
#ifndef HAR_DSP_H
#define HAR_DSP_H

#include "model_params.h"

/* ---- median 3, dang streaming, TRE 1 MAU ----
 * har/dsp.py median3() can ca mau truoc va mau sau, nen ban streaming chi xuat duoc
 * mau thu k sau khi da nhan mau k+1. Cu the:
 *     push mau 0  -> chua co gi          (tra ve 0)
 *     push mau 1  -> xuat chi so 0 = x[0]            (giu nguyen, nhu Python)
 *     push mau k  -> xuat chi so k-1 = median(x[k-2], x[k-1], x[k])
 * Nho quy uoc nay chuoi ra TRUNG KHOP TUNG BIT voi har/dsp.py median3().
 */
typedef struct {
    float a, b;      /* 2 mau gan nhat */
    int   n;         /* so mau da nhan */
} har_median3_t;

void har_median3_reset(har_median3_t *st);
int  har_median3_push(har_median3_t *st, float x, float *out);

/* ---- cascade biquad, transposed direct form II, giong scipy.signal.sosfilt ----
 * Trang thai khoi tao = zi[section] * mau dau tien CUA TIN HIEU GOC, nho do khong
 * co transient. Bo qua thi 4 cua so dau sau moi lan cap dien sai nhan.
 */
typedef struct {
    float z[HAR_N_SOS][2];
    int   primed;
} har_biquad_t;

void  har_biquad_reset(har_biquad_t *st);
float har_biquad_push(har_biquad_t *st, const float sos[HAR_N_SOS][6],
                      const float zi[HAR_N_SOS][2], float x);

/* ---- pho: |FFT(x, 128)| bin 1..64, BO bin DC, chia HAR_SPEC_SCALE ----
 * out phai co it nhat HAR_N_SPECTRUM phan tu. Dung buffer tinh noi bo -> KHONG
 * goi lai tu 2 luong song song.
 */
void har_spectrum(const float *x, float *out);

/* ---- cac phep thong ke tren mot kenh ---- */
float har_mean(const float *x, int n);
float har_std(const float *x, int n);          /* chia N, khong chia N-1 */
float har_mad(const float *x, int n);          /* do lech tuyet doi TRUNG BINH */
float har_max(const float *x, int n);
float har_min(const float *x, int n);
float har_energy(const float *x, int n);       /* sum(x^2) / n */
float har_iqr(const float *x, int n);          /* quy uoc MATLAB (i-0.5)/n */
float har_entropy(const float *x, int n);      /* histogram 256 o CO DINH tren [0,1] */
float har_skew(const float *x, int n);
float har_kurt(const float *x, int n);
float har_corr(const float *x, const float *y, int n);
float har_mean_freq(const float *x, int n);    /* trong tam pho, trong so |X| */
void  har_burg(const float *x, int n, float *out);   /* he so AR bac HAR_AR_ORDER */
float har_angle(const float *v, const float *g);     /* goc 2 vector 3 chieu, tru pi/2 */

#endif /* HAR_DSP_H */
