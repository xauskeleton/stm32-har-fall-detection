/* Chon 160 cot -> chuan hoa -> nhan ma tran 12x160 -> argmax.
 *
 * Ban tham chieu Python: har/model.py predict(). Da doi chieu tren 3162 cua so tap
 * test: 0 cua so lech nhan (xem docs/DECISIONS.md muc 7).
 */
#ifndef HAR_CLASSIFIER_H
#define HAR_CLASSIFIER_H

#include "model_params.h"

/* feat co HAR_N_FEATURES_ALL phan tu (do har_features() sinh ra).
 * Tra ve nhan 1..HAR_N_CLASSES. scores != NULL -> ghi them HAR_N_CLASSES diem tho
 * (chua softmax) de dat nguong tin cay neu can.
 */
int har_classify(const float *feat, float *scores);

#endif /* HAR_CLASSIFIER_H */
