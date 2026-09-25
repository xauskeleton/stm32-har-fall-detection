/* Xem har_features.h. Ban tham chieu Python: har/dsp.py extract_features(). */
#include "har_features.h"

#include <math.h>

#include "har_dsp.h"

#define W      HAR_WINDOW
#define NSPEC  HAR_N_SPECTRUM

/* Nhom nao duoc lay pho 3 truc / pho cua do lon - khop cfg.SIGNALS_SPEC va
 * cfg.SIGNALS_SPEC_MAG trong har/config.py. */
static const int SPEC_SIG[3] = { HAR_SIG_BODY_ACC, HAR_SIG_BODY_ACC_JERK, HAR_SIG_GYRO };
static const int SPEC_MAG_SIG[4] = { HAR_SIG_BODY_ACC, HAR_SIG_BODY_ACC_JERK,
                                     HAR_SIG_GYRO, HAR_SIG_GYRO_JERK };
static const int ANGLE_SIG[4] = { HAR_SIG_BODY_ACC, HAR_SIG_BODY_ACC_JERK,
                                  HAR_SIG_GYRO, HAR_SIG_GYRO_JERK };

/* buffer tinh - khong malloc trong duong chay real-time */
static float mag[W], sp[3][NSPEC];

void har_features(const float win[HAR_N_SIGNALS][3][W], float *out)
{
    int o = 0;
    float ar[HAR_AR_ORDER];

    /* === 5 nhom tin hieu 3 truc === */
    for (int g = 0; g < HAR_N_SIGNALS; g++) {
        int stable = (g != HAR_SIG_GRAVITY);
        for (int ax = 0; ax < 3; ax++) {
            const float *s = win[g][ax];
            out[o++] = har_mean(s, W);            /* bias / DC */
            out[o++] = har_std(s, W);             /* do bien thien */
            out[o++] = har_mad(s, W);             /* do phan tan */
            out[o++] = har_max(s, W);             /* peak duong */
            out[o++] = har_min(s, W);             /* peak am */
            out[o++] = har_energy(s, W);          /* nang luong trung binh */
            out[o++] = har_iqr(s, W);             /* phan tan robust */
            if (stable) {
                out[o++] = har_entropy(s, W);     /* do "loan" cua bien do */
                har_burg(s, W, ar);
                for (int j = 0; j < HAR_AR_ORDER; j++) out[o++] = ar[j];   /* dang song */
            }
        }
        float sma = 0.0f;
        for (int ax = 0; ax < 3; ax++)
            for (int i = 0; i < W; i++) sma += fabsf(win[g][ax][i]);
        out[o++] = sma / (float)W;                /* do lon trung binh 3 truc */
        if (stable) {
            out[o++] = har_corr(win[g][0], win[g][1], W);   /* phoi hop 2 truc */
            out[o++] = har_corr(win[g][0], win[g][2], W);
            out[o++] = har_corr(win[g][1], win[g][2], W);
        }
    }

    /* === 5 kenh do lon mien thoi gian === */
    for (int g = 0; g < HAR_N_SIGNALS; g++) {
        for (int i = 0; i < W; i++)
            mag[i] = sqrtf(win[g][0][i] * win[g][0][i]
                           + win[g][1][i] * win[g][1][i]
                           + win[g][2][i] * win[g][2][i]);   /* bat bien voi huong dat */
        out[o++] = har_mean(mag, W);
        out[o++] = har_std(mag, W);
        out[o++] = har_mad(mag, W);
        out[o++] = har_max(mag, W);
        out[o++] = har_min(mag, W);
        out[o++] = har_energy(mag, W);
        out[o++] = har_iqr(mag, W);
        if (g != HAR_SIG_GRAVITY) {
            out[o++] = har_entropy(mag, W);
            har_burg(mag, W, ar);
            for (int j = 0; j < HAR_AR_ORDER; j++) out[o++] = ar[j];
        }
    }

    /* === 3 pho 3 truc === */
    for (int q = 0; q < 3; q++) {
        int g = SPEC_SIG[q];
        for (int ax = 0; ax < 3; ax++) har_spectrum(win[g][ax], sp[ax]);
        for (int ax = 0; ax < 3; ax++) {
            const float *s = sp[ax];
            out[o++] = har_mean(s, NSPEC);
            out[o++] = har_std(s, NSPEC);
            out[o++] = har_mad(s, NSPEC);
            out[o++] = har_max(s, NSPEC);         /* bin manh nhat */
            out[o++] = har_min(s, NSPEC);
            out[o++] = har_energy(s, NSPEC);      /* nang luong pho */
            out[o++] = har_iqr(s, NSPEC);
            out[o++] = har_entropy(s, NSPEC);     /* pho bet hay co dinh */
            out[o++] = har_mean_freq(s, NSPEC);   /* trong tam pho */
            out[o++] = har_skew(s, NSPEC);
            out[o++] = har_kurt(s, NSPEC);
        }
        float sma = 0.0f;
        for (int ax = 0; ax < 3; ax++)
            for (int k = 0; k < NSPEC; k++) sma += fabsf(sp[ax][k]);
        out[o++] = sma / (float)NSPEC;
        for (int ax = 0; ax < 3; ax++)
            for (int b = 0; b < HAR_N_BANDS; b++) {
                float e = 0.0f;
                for (int k = HAR_BANDS[b][0] - 1; k < HAR_BANDS[b][1]; k++)
                    e += sp[ax][k] * sp[ax][k];
                out[o++] = e;                     /* nang luong tung dai tan */
            }
    }

    /* === 4 pho cua do lon === */
    for (int q = 0; q < 4; q++) {
        int g = SPEC_MAG_SIG[q];
        for (int i = 0; i < W; i++)
            mag[i] = sqrtf(win[g][0][i] * win[g][0][i]
                           + win[g][1][i] * win[g][1][i]
                           + win[g][2][i] * win[g][2][i]);
        har_spectrum(mag, sp[0]);
        const float *s = sp[0];
        out[o++] = har_mean(s, NSPEC);
        out[o++] = har_std(s, NSPEC);
        out[o++] = har_mad(s, NSPEC);
        out[o++] = har_max(s, NSPEC);
        out[o++] = har_min(s, NSPEC);
        out[o++] = har_energy(s, NSPEC);
        out[o++] = har_iqr(s, NSPEC);
        out[o++] = har_entropy(s, NSPEC);
        out[o++] = har_mean_freq(s, NSPEC);
        out[o++] = har_skew(s, NSPEC);
        out[o++] = har_kurt(s, NSPEC);
    }

    /* === 7 goc so voi huong gravity: phan biet tu the dung / ngoi / nam === */
    float gm[3], vm[3];
    for (int ax = 0; ax < 3; ax++) gm[ax] = har_mean(win[HAR_SIG_GRAVITY][ax], W);
    for (int q = 0; q < 4; q++) {
        for (int ax = 0; ax < 3; ax++) vm[ax] = har_mean(win[ANGLE_SIG[q]][ax], W);
        out[o++] = har_angle(vm, gm);
    }
    for (int ax = 0; ax < 3; ax++) {
        float e[3] = { 0.0f, 0.0f, 0.0f };
        e[ax] = 1.0f;
        out[o++] = har_angle(e, gm);
    }
}
