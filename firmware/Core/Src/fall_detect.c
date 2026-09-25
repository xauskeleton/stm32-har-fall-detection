/* Xem fall_detect.h. BAN SAO 1-1 cua har/fall.py FallDetector.update().
 * Sua mot ben thi PHAI sua ben kia. Nguong nam trong model_params.h (sinh tu Python).
 */
#include "fall_detect.h"

/* Nhom nhan - khop har/config.py */
static int is_active(int label)      /* chua nam */
{
    return label >= HAR_WALKING && label <= HAR_STANDING;    /* 1..5 */
}

static int is_upright(int label)     /* dung / di bo */
{
    return label == HAR_WALKING || label == HAR_WALKING_UPSTAIRS
           || label == HAR_WALKING_DOWNSTAIRS || label == HAR_STANDING;
}

static int is_lie_down_transition(int label)   /* nam xuong chu dong */
{
    return label == HAR_SIT_TO_LIE || label == HAR_STAND_TO_LIE;
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
}

/* Ve IDLE nhung GIU prev_label va guard. */
static void reset_soft(fall_state_t *st)
{
    st->state = FALL_STATE_IDLE;
    st->timer = 0;
    st->laying = 0;
    st->recover = 0;
}

static int is_shock(float a_min, float a_max)
{
    /* shock_rule = 'both': phai co CA roi tu do VA cu dap. a_min/a_max tinh tren
     * gia toc TONG da loc median 3 (KHONG loc lowpass - can giu nguyen dinh). */
    return (a_min <= FALL_FREEFALL_G) && (a_max >= FALL_IMPACT_G);
}

/* Nhay thang tu tu the chua nam sang LAYING ma khong qua nhan *_TO_LIE.
 * active_run = so cua so chua-nam lien tiep NGAY TRUOC cua so nay: doi it nhat
 * FALL_UPRIGHT_MIN, neu khong thi mot cua so doan sai don le cung du bao dong. */
static int abrupt_to_laying(const fall_state_t *st, int label, int active_run)
{
    return label == HAR_LAYING
           && is_active(st->prev_label)
           && active_run >= FALL_UPRIGHT_MIN
           && st->guard == 0;
}

int fall_update(fall_state_t *st, int label, float a_min, float a_max)
{
    int event = FALL_EVENT_NONE;

    int active_run = st->active_run;          /* do dai chuoi chua-nam TRUOC cua so nay */
    st->active_run = is_active(label) ? active_run + 1 : 0;

    if (st->guard > 0) st->guard--;
    if (is_lie_down_transition(label)) st->guard = FALL_VOLUNTARY_GUARD;

    int shock = is_shock(a_min, a_max);

    switch (st->state) {
    case FALL_STATE_IDLE:
        if (shock || abrupt_to_laying(st, label, active_run)) {
            st->state = FALL_STATE_SHOCK;
            st->timer = FALL_CONFIRM_WIN;
            st->laying = (label == HAR_LAYING) ? 1 : 0;
            if (st->laying >= FALL_LAYING_CONFIRM) {
                st->state = FALL_STATE_FALLEN;
                event = FALL_EVENT_FALL;
            }
        }
        break;

    case FALL_STATE_SHOCK:
        st->timer--;
        if (is_lie_down_transition(label) || st->guard > 0) {
            reset_soft(st);                   /* nam chu dong -> huy nghi ngo */
        } else if (label == HAR_LAYING) {
            st->laying++;
            if (st->laying >= FALL_LAYING_CONFIRM) {
                st->state = FALL_STATE_FALLEN;
                st->recover = 0;
                event = FALL_EVENT_FALL;
            }
        } else if (is_upright(label)) {
            reset_soft(st);                   /* da dung day -> khong phai te nang */
        } else if (st->timer <= 0) {
            reset_soft(st);
        }
        break;

    case FALL_STATE_FALLEN:
        if (label == HAR_LAYING) {
            st->recover = 0;
        } else {
            st->recover++;
            if (st->recover >= FALL_RECOVER_WIN) {
                reset_soft(st);
                event = FALL_EVENT_RECOVERED;
            }
        }
        break;

    default:
        break;
    }

    st->prev_label = label;
    return event;
}
