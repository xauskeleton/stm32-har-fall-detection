/* Xem fall_detect.h. BAN SAO 1-1 cua ml/core/fall.py FallDetector.update().
 * Sua mot ben thi PHAI sua ben kia. Nguong nam trong model_params.h (sinh tu Python).
 */
#include "fall_detect.h"

/* abrupt_to_laying() bo kiem tra guard vi FALL_UPRIGHT_MIN >= FALL_VOLUNTARY_GUARD
 * dam bao guard da het khi du dieu kien. Doi tham so ma pha dieu nay thi bao loi. */
#if FALL_UPRIGHT_MIN < FALL_VOLUNTARY_GUARD
#error "FALL_UPRIGHT_MIN phai >= FALL_VOLUNTARY_GUARD (xem abrupt_to_laying)"
#endif

/* Nhom nhan - khop ml/core/config.py */
static int is_upright(int label)     /* dung / di bo (KHONG gom ngoi) */
{
    return label == HAR_WALKING || label == HAR_WALKING_UPSTAIRS
           || label == HAR_WALKING_DOWNSTAIRS || label == HAR_STANDING;
}

static int is_lie_down_transition(int label)   /* nam xuong chu dong */
{
    return label == HAR_SIT_TO_LIE || label == HAR_STAND_TO_LIE;
}

static int is_get_up_transition(int label)     /* tu nam ngoi / dung day */
{
    return label == HAR_LIE_TO_SIT || label == HAR_LIE_TO_STAND;
}

void fall_reset(fall_state_t *st)
{
    st->state = FALL_STATE_IDLE;
    st->prev_label = 0;
    st->guard = 0;
    st->timer = 0;
    st->laying = 0;
    st->recover = 0;
    st->active_run = 0;
    st->by_shock = 0;
    st->shock_upright = 0;
}

/* Ve IDLE nhung GIU prev_label va guard. */
static void reset_soft(fall_state_t *st)
{
    st->state = FALL_STATE_IDLE;
    st->timer = 0;
    st->laying = 0;
    st->recover = 0;
    st->by_shock = 0;
    st->shock_upright = 0;
}

static int is_shock(float a_min, float a_max)
{
    /* shock_rule = 'both': phai co CA roi tu do VA cu dap. a_min/a_max tinh tren
     * gia toc TONG da loc median 3 (KHONG loc lowpass - can giu nguyen dinh). */
    return (a_min <= FALL_FREEFALL_G) && (a_max >= FALL_IMPACT_G);
}

/* Nhay thang tu DUNG / DI sang LAYING ma khong qua nhan *_TO_LIE.
 * active_run = so cua so dung/di lien tiep NGAY TRUOC cua so nay: doi it nhat
 * FALL_UPRIGHT_MIN, va phai co dau hieu tut/va dap nhe.
 *
 * - Khong tinh SITTING: ngoi nga lung ra ghe / giuong ma model sot nhan SIT_TO_LIE
 *   la nguon bao nham chinh. Nga tu tu the ngoi van bat duoc qua nhanh soc.
 * - Khong can kiem tra guard o day: nhan *_TO_LIE khong phai dung/di nen tu dua
 *   active_run ve 0; can them >= FALL_UPRIGHT_MIN (= FALL_VOLUNTARY_GUARD) cua so
 *   dung/di nua thi guard da het. Guard van dung o trang thai SHOCK. */
static int abrupt_to_laying(const fall_state_t *st, int label, int active_run,
                            float a_min, float a_max)
{
    int has_motion = (a_min <= FALL_ABRUPT_DROP_G) || (a_max >= FALL_ABRUPT_PEAK_G);
    return label == HAR_LAYING
           && is_upright(st->prev_label)
           && active_run >= FALL_UPRIGHT_MIN
           && has_motion;
}

int fall_update(fall_state_t *st, int label, float a_min, float a_max)
{
    int event = FALL_EVENT_NONE;

    int active_run = st->active_run;          /* do dai chuoi dung/di TRUOC cua so nay */
    st->active_run = is_upright(label) ? active_run + 1 : 0;

    int guard_before = st->guard;             /* guard TRUOC cua so nay */
    if (st->guard > 0) st->guard--;
    if (is_lie_down_transition(label)) st->guard = FALL_VOLUNTARY_GUARD;

    int shock = is_shock(a_min, a_max);

    /* Nhan "nam xuong" ma co roi + dap >= FALL_LIEDOWN_IMPACT_G, truoc do khong co nhan
     * *_TO_LIE nao: nam chu dong khong bao gio dap manh vay -> la cu nga. */
    int liedown_shock = is_lie_down_transition(label) && guard_before == 0
                        && a_min <= FALL_FREEFALL_G && a_max >= FALL_LIEDOWN_IMPACT_G;

    switch (st->state) {
    case FALL_STATE_IDLE:
        if (shock || liedown_shock || abrupt_to_laying(st, label, active_run, a_min, a_max)) {
            st->state = FALL_STATE_SHOCK;
            st->timer = FALL_CONFIRM_WIN;
            st->laying = (label == HAR_LAYING) ? 1 : 0;
            /* Soc that ma TRUOC DO khong co nhan *_TO_LIE nao = te nga. Model chua
             * tung thay te nga nen hay gan cu nga la STAND_TO_LIE; nhan do den SAU cu
             * soc thi khong duoc coi la nam chu dong. Do nguoi xuong giuong thi nhan
             * *_TO_LIE den TRUOC cu soc -> guard_before > 0 -> van huy nhu cu. */
            st->by_shock = (shock || liedown_shock) && (guard_before == 0);
            st->shock_upright = 0;
            if (st->laying >= FALL_LAYING_CONFIRM) {
                st->state = FALL_STATE_FALLEN;
                event = FALL_EVENT_FALL;
            }
        }
        break;

    case FALL_STATE_SHOCK: {
        /* Vao bang soc that: can dung/di FALL_SHOCK_UPRIGHT_WIN cua so lien tiep moi
         * huy; vao bang nhanh nhan: 1 cua so nhu cu. */
        int need_upright = st->by_shock ? FALL_SHOCK_UPRIGHT_WIN : 1;
        st->timer--;
        st->shock_upright = is_upright(label) ? st->shock_upright + 1 : 0;
        if (!st->by_shock && (is_lie_down_transition(label) || st->guard > 0)) {
            reset_soft(st);                   /* nam chu dong -> huy nghi ngo */
        } else if (label == HAR_LAYING) {
            st->laying++;
            if (st->laying >= FALL_LAYING_CONFIRM) {
                st->state = FALL_STATE_FALLEN;
                st->recover = 0;
                event = FALL_EVENT_FALL;
            }
        } else if (st->shock_upright >= need_upright) {
            reset_soft(st);                   /* da dung day -> khong phai te nang */
        } else if (st->timer <= 0) {
            reset_soft(st);
        }
        break;
    }

    case FALL_STATE_FALLEN:
        /* Chi coi la da dung day khi thay DONG TAC DAY (LIE_TO_SIT / LIE_TO_STAND)
         * roi khong quay lai LAYING. Ngoi / nhan khac khi chua thay dong tac day
         * (vd ngoi bet duoi san sau khi nga) van giu bao dong. */
        if (is_get_up_transition(label)) {
            st->recover++;
        } else if (label == HAR_LAYING) {
            st->recover = 0;
        } else if (st->recover > 0) {
            st->recover++;                    /* dang day do: dem tiep */
        }
        /* Du phong khi model sot nhan dong tac day: da dung / di lien tuc
         * >= FALL_UPRIGHT_MIN cua so (~5 s) thi cung tat, tranh coi keu mai. */
        if (st->recover >= FALL_RECOVER_WIN || st->active_run >= FALL_UPRIGHT_MIN) {
            reset_soft(st);
            event = FALL_EVENT_RECOVERED;
        }
        break;

    default:
        break;
    }

    st->prev_label = label;
    return event;
}
