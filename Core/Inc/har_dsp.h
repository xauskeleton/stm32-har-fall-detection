#ifndef HAR_DSP_H
#define HAR_DSP_H

#include <stdint.h>
#include <stdbool.h>
#include "model_params.h"

#define DSP_FS          50
#define DSP_WINDOW      128
#define DSP_STEP        64
#define DSP_N_FEATURES  120

// Cấu trúc chứa 1 mẫu dữ liệu cảm biến (Gia tốc g, Góc quay dps)
typedef struct {
    float ax;
    float ay;
    float az;
    float gx;
    float gy;
    float gz;
} IMU_Sample_t;

void DSP_Init(void);
bool DSP_PushSample(const IMU_Sample_t *sample); // Trả về true khi có đủ 64 mẫu mới
void DSP_ExtractFeatures(float *out_features);

// Hàm lấy mẫu gia tốc mới nhất phục vụ Shock Detection
float DSP_GetLatestAccMag(void);

#endif /* HAR_DSP_H */
