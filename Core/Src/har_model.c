#include "har_model.h"
#include "arm_math.h"
#include <string.h>
#include <math.h>

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

    // 3. Tính Softmax để có mảng Xác suất (Probabilities)
    float max_logit = 0.0f;
    uint32_t max_logit_idx = 0;
    arm_max_f32(out_pred->scores, MODEL_N_CLASSES, &max_logit, &max_logit_idx);
    
    float sum_exp = 0.0f;
    for (int i = 0; i < MODEL_N_CLASSES; i++) {
        out_pred->scores[i] = expf(out_pred->scores[i] - max_logit);
        sum_exp += out_pred->scores[i];
    }
    for (int i = 0; i < MODEL_N_CLASSES; i++) {
        out_pred->scores[i] /= sum_exp;
    }

    // 4. Áp dụng Threshold ưu tiên cho lớp LAYING (Index 5)
    #define LAYING_INDEX 5
    #define LAYING_THRESHOLD 0.45f
    
    uint32_t max_idx = 0;
    float max_val = 0.0f;

    if (out_pred->scores[LAYING_INDEX] > LAYING_THRESHOLD) {
        max_idx = LAYING_INDEX;
        max_val = out_pred->scores[LAYING_INDEX];
    } else {
        // Che nhãn LAYING (Gán -1.0f vì xác suất luôn >= 0)
        float temp_score = out_pred->scores[LAYING_INDEX];
        out_pred->scores[LAYING_INDEX] = -1.0f;
        
        arm_max_f32(out_pred->scores, MODEL_N_CLASSES, &max_val, &max_idx);
        
        // Phục hồi lại xác suất cũ
        out_pred->scores[LAYING_INDEX] = temp_score;
    }

    out_pred->predicted_class = (uint8_t)(max_idx + 1); // ID từ 1 đến 12
    out_pred->class_name = MODEL_CLASS_NAMES[max_idx];
    out_pred->max_score = max_val;
}
