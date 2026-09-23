#include "har_model.h"
#include "arm_math.h"
#include <string.h>

void AI_Init(void) {
    // Không cần cấp phát bộ nhớ động
}

void AI_Predict(const float *features, AI_Prediction_t *out_pred) {
    static float norm_features[MODEL_N_FEATURES];

    // 1. Chuẩn hóa Z-Score
    for (int i = 0; i < MODEL_N_FEATURES; i++) {
        norm_features[i] = (features[i] - MODEL_SCALER_MEAN[i]) / MODEL_SCALER_SCALE[i];
    }

    // 2. Logistic Regression (Linear Logits) dùng CMSIS-DSP arm_dot_prod_f32
    for (int c = 0; c < MODEL_N_CLASSES; c++) {
        float dot_prod = 0.0f;
        arm_dot_prod_f32(norm_features, &MODEL_WEIGHTS[c * MODEL_N_FEATURES], MODEL_N_FEATURES, &dot_prod);
        out_pred->scores[c] = dot_prod + MODEL_BIAS[c];
    }

    // 3. Argmax tìm nhãn có logit score cao nhất
    uint32_t max_idx = 0;
    float max_val = 0.0f;
    arm_max_f32(out_pred->scores, MODEL_N_CLASSES, &max_val, &max_idx);

    out_pred->predicted_class = (uint8_t)(max_idx + 1); // ID từ 1 đến 12
    out_pred->class_name = MODEL_CLASS_NAMES[max_idx];
    out_pred->max_score = max_val;
}
