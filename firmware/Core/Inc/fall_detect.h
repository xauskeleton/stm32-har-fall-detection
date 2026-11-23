/* Phat hien te nga TU CHUOI NHAN dau ra cua bo phan loai 12 lop.
 *
 * BAN SAO 1-1 cua ml/core/fall.py. Sua mot ben thi PHAI sua ben kia.
 *
 *   Nam xuong binh thuong :  STANDING -> STAND_TO_LIE -> LAYING
 *                            SITTING  -> SIT_TO_LIE   -> LAYING
 *   Te nga                :  WALKING/STANDING -> (khong co nhan transition) -> LAYING
 *                            kem mot cu soc gia toc (roi tu do roi dap san)
 *
 *   IDLE   --soc / nhay thang sang LAYING--> SHOCK
 *   SHOCK  --LAYING lien tiep du lau-------> FALLEN   (BAO DONG)
 *   SHOCK  --thay nhan *_TO_LIE-----------> IDLE      (nam chu dong, khong bao)
 *   SHOCK  --quay lai dung/di-------------> IDLE      (vap nhe, tu dung day)
 *   FALLEN --roi khoi LAYING du lau-------> IDLE      (da dung day / duoc do day)
 */
#ifndef FALL_DETECT_H
#define FALL_DETECT_H

#include "model_params.h"

#define FALL_STATE_IDLE    0
#define FALL_STATE_SHOCK   1
#define FALL_STATE_FALLEN  2

#define FALL_EVENT_NONE       0
#define FALL_EVENT_FALL       1
#define FALL_EVENT_RECOVERED  2

/* Nguong chuyen dong toi thieu cho nhanh nhay thang sang LAYING (chong bao gia khi ngoi nga lung) */
#define FALL_ABRUPT_DROP_G    0.75f
#define FALL_ABRUPT_PEAK_G    1.50f

/* Vao SHOCK bang cu soc that: phai dung/di LIEN TIEP bao nhieu cua so moi coi la da
 * dung day. Cua so ngay sau cua so soc chong 50% len cu nga, model hay doan la
 * WALKING_DOWNSTAIRS -> 1 cua so dung/di khong du tin. */
#define FALL_SHOCK_UPRIGHT_WIN 2

typedef struct {
    int state;
    int prev_label;
    int guard;         /* so cua so con lai coi LAYING la "nam chu dong" */
    int timer;         /* dem nguoc trong SHOCK */
    int laying;        /* so cua so LAYING lien tiep */
    int recover;
    int active_run;    /* so cua so DUNG / DI LIEN TIEP vua roi (khong gom ngoi) */
    int by_shock;      /* 1 = vao SHOCK bang cu soc that khi guard dang = 0 */
    int shock_upright; /* trong SHOCK: so cua so dung/di lien tiep ke tu luc vao */
} fall_state_t;

void fall_reset(fall_state_t *st);

/* Nap tung cua so theo thu tu thoi gian.
 * label  : 1..HAR_N_CLASSES do har_classify() tra ve
 * a_min  : |a| nho nhat trong cua so (g), tren gia toc TONG da loc median 3
 * a_max  : |a| lon nhat trong cua so (g)
 * Tra ve FALL_EVENT_*.
 */
int fall_update(fall_state_t *st, int label, float a_min, float a_max);

#endif /* FALL_DETECT_H */
