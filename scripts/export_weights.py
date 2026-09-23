#!/usr/bin/env python3
import joblib
import numpy as np
import os

def main():
    base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    scaler_path = os.path.join(base_dir, 'models', 'scaler.pkl')
    clf_path = os.path.join(base_dir, 'models', 'classifier.pkl')

    scaler = joblib.load(scaler_path)
    clf = joblib.load(clf_path)

    mean = scaler.mean_.astype(np.float32)
    scale = scaler.scale_.astype(np.float32)
    coef = clf.coef_.astype(np.float32)       # shape (12, 120)
    intercept = clf.intercept_.astype(np.float32) # shape (12,)

    inc_dir = os.path.join(os.path.dirname(base_dir), 'Core', 'Inc')
    src_dir = os.path.join(os.path.dirname(base_dir), 'Core', 'Src')

    h_path = os.path.join(inc_dir, 'model_params.h')
    c_path = os.path.join(src_dir, 'model_params.c')

    # Generate model_params.h
    with open(h_path, 'w') as f:
        f.write('''#ifndef MODEL_PARAMS_H
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
''')

    # Generate model_params.c
    with open(c_path, 'w') as f:
        f.write('#include "model_params.h"\n\n')

        # Mean
        f.write('const float MODEL_SCALER_MEAN[MODEL_N_FEATURES] = {\n    ')
        f.write(', '.join(f'{x:.8e}f' for x in mean))
        f.write('\n};\n\n')

        # Scale
        f.write('const float MODEL_SCALER_SCALE[MODEL_N_FEATURES] = {\n    ')
        f.write(', '.join(f'{x:.8e}f' for x in scale))
        f.write('\n};\n\n')

        # Weights (flattened 12 * 120)
        f.write('const float MODEL_WEIGHTS[MODEL_N_CLASSES * MODEL_N_FEATURES] = {\n')
        for i, row in enumerate(coef):
            f.write('    /* Class ' + str(i+1) + ' */\n    ')
            f.write(', '.join(f'{x:.8e}f' for x in row))
            if i < len(coef) - 1:
                f.write(',\n')
            else:
                f.write('\n')
        f.write('};\n\n')

        # Bias
        f.write('const float MODEL_BIAS[MODEL_N_CLASSES] = {\n    ')
        f.write(', '.join(f'{x:.8e}f' for x in intercept))
        f.write('\n};\n\n')

        # Class Names
        f.write('''const char* const MODEL_CLASS_NAMES[MODEL_N_CLASSES] = {
    "WALKING", "WALKING_UPSTAIRS", "WALKING_DOWNSTAIRS",
    "SITTING", "STANDING", "LAYING",
    "STAND_TO_SIT", "SIT_TO_STAND", "SIT_TO_LIE",
    "LIE_TO_SIT", "STAND_TO_LIE", "LIE_TO_STAND"
};
''')

    print(f"Exported successfully to:\n  - {h_path}\n  - {c_path}")

if __name__ == '__main__':
    main()
