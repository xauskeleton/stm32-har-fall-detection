/* Doi chieu C voi Python bang golden_vectors.h (sinh tu scripts/export_model.py).
 *
 * TIEU CHI CHINH la NHAN: phai khop TUNG cua so. Sai so dac trung chi de chan doan;
 * GOLDEN_TOL la so DO THUC TE, khong phai muc tieu (xem docs/DECISIONS.md muc 7).
 *
 * Chay tren PC:  make -C firmware test
 */
#include <math.h>
#include <stdio.h>

#include "golden_vectors.h"
#include "har_app.h"
#include "har_classifier.h"
#include "har_pipeline.h"

int main(void)
{
    int n_win = 0, n_label_bad = 0, n_feat_bad = 0;
    float worst = 0.0f;
    int worst_col = -1, worst_win = -1;

    har_pipeline_reset();

    for (int i = 0; i < GOLDEN_N_SAMPLES; i++) {
        har_result_t r;
        if (!har_pipeline_push(GOLDEN_ACC[i], GOLDEN_GYRO[i], &r)) continue;
        if (n_win >= GOLDEN_N_WINDOWS) break;

        if (r.label != GOLDEN_LABELS[n_win]) {
            n_label_bad++;
            printf("  cua so %d: nhan C = %2d (%s), Python = %2d (%s)\n",
                   n_win, r.label, har_label_name(r.label),
                   GOLDEN_LABELS[n_win], har_label_name(GOLDEN_LABELS[n_win]));
        }

        /* GOLDEN_FEATURES giu HAR_N_FEATURES cot DA CHON, chua chuan hoa */
        const float *feat = har_pipeline_features();
        for (int j = 0; j < HAR_N_FEATURES; j++) {
            float d = fabsf(feat[HAR_SELECTED[j]] - GOLDEN_FEATURES[n_win][j]);
            if (d > worst) { worst = d; worst_col = j; worst_win = n_win; }
            if (d > GOLDEN_TOL) n_feat_bad++;
        }
        n_win++;
    }

    /* --- lan 2: qua har_app (hoan vi + kep + state machine te nga) ---
     * Bien dich test voi -DHAR_AXIS_PERMUTE=0 vi golden vectors da o he truc HAPT. */
    int n_app_bad = 0, n_app_win = 0, n_fall = 0;
    har_app_reset();
    for (int i = 0; i < GOLDEN_N_SAMPLES; i++) {
        har_app_event_t ev;
        if (!har_app_on_sample(GOLDEN_ACC[i], GOLDEN_GYRO[i], &ev)) continue;
        if (n_app_win >= GOLDEN_N_WINDOWS) break;
        if (ev.label != GOLDEN_LABELS[n_app_win]) n_app_bad++;
        if (ev.fall) n_fall++;
        n_app_win++;
    }

    int ok = (n_win == GOLDEN_N_WINDOWS) && (n_label_bad == 0) && (n_feat_bad == 0)
             && (n_app_win == GOLDEN_N_WINDOWS) && (n_app_bad == 0);
    printf("\n=== doi chieu C vs Python ===\n");
    printf("  cua so kiem tra       : %d / %d\n", n_win, GOLDEN_N_WINDOWS);
    printf("  nhan lech             : %d\n", n_label_bad);
    printf("  dac trung vuot nguong : %d / %d  (nguong %.3g)\n",
           n_feat_bad, n_win * HAR_N_FEATURES, (double)GOLDEN_TOL);
    if (worst_col >= 0)
        printf("  sai so lon nhat       : %.3g  (cua so %d, cot chon thu %d)\n",
               (double)worst, worst_win, worst_col);
    printf("  qua har_app           : %d / %d cua so, %d nhan lech, %d su kien te nga\n",
           n_app_win, GOLDEN_N_WINDOWS, n_app_bad, n_fall);
    printf("  ket qua               : %s\n", ok ? "DAT" : "KHONG DAT");
    return ok ? 0 : 1;
}
