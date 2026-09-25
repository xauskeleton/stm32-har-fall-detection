/* Xem har_classifier.h. */
#include "har_classifier.h"

int har_classify(const float *feat, float *scores)
{
    static float x[HAR_N_FEATURES];

    /* chon cot roi chuan hoa: nhan voi 1/scale de khong phai CHIA */
    for (int i = 0; i < HAR_N_FEATURES; i++)
        x[i] = (feat[HAR_SELECTED[i]] - HAR_SCALER_MEAN[i]) * HAR_SCALER_INV_SCALE[i];

    int best = 0;
    float best_score = 0.0f;
    for (int k = 0; k < HAR_N_CLASSES; k++) {
        float s = HAR_BIAS[k];
        for (int i = 0; i < HAR_N_FEATURES; i++) s += HAR_WEIGHTS[k][i] * x[i];
        if (scores) scores[k] = s;
        if (k == 0 || s > best_score) { best_score = s; best = k; }
    }
    return best + 1;            /* nhan HAPT dem tu 1 */
}
