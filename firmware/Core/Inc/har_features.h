/* 516 dac trung cho mot cua so 128 mau.
 *
 * Ban tham chieu Python: har/dsp.py extract_features(). Thu tu PHAI giu nguyen -
 * dao thu tu la model sai IM LANG, khong co thong bao loi nao.
 * har/dsp.py feature_names() in ra dung thu tu de doi chieu.
 */
#ifndef HAR_FEATURES_H
#define HAR_FEATURES_H

#include "model_params.h"

/* win[nhom][truc][mau], nhom theo HAR_SIG_*, out co HAR_N_FEATURES_ALL phan tu.
 *
 * Nhom HAR_SIG_GRAVITY bo entropy / arCoeff / correlation: tin hieu gan hang so nen
 * 3 phep do mat dieu kien so trong float32 (xem docs/DECISIONS.md muc 7).
 * Moi nhom bo max_ind: argmax la dau ra roi rac, lech 1e-7 la nhay bin.
 */
void har_features(const float win[HAR_N_SIGNALS][3][HAR_WINDOW], float *out);

#endif /* HAR_FEATURES_H */
