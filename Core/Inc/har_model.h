#ifndef HAR_MODEL_H
#define HAR_MODEL_H

#include <stdint.h>
#include "model_params.h"

typedef struct {
    uint8_t predicted_class; // Label ID 1..12
    const char *class_name;
    float max_score;
    float scores[MODEL_N_CLASSES];
} AI_Prediction_t;

void AI_Init(void);
void AI_Predict(const float *features, AI_Prediction_t *out_pred);

#endif /* HAR_MODEL_H */
