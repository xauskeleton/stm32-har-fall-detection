#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>

#define MODEL_N_FEATURES 120
#define MODEL_N_CLASSES  12

extern const float MODEL_SCALER_MEAN[MODEL_N_FEATURES];
extern const float MODEL_SCALER_SCALE[MODEL_N_FEATURES];
extern const float MODEL_WEIGHTS[MODEL_N_CLASSES * MODEL_N_FEATURES];
extern const float MODEL_BIAS[MODEL_N_CLASSES];

extern const char* const MODEL_CLASS_NAMES[MODEL_N_CLASSES];

#endif /* MODEL_PARAMS_H */
