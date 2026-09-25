/* Xem har_app.h. */
#include "har_app.h"

#include "har_pipeline.h"

static fall_state_t fall_st;

/* Uoc luong gravity de hieu chuan: loc thong thap 1 chieu he so 0.01 (~2 s o 50 Hz).
 * Khong lien quan den duong tinh dac trung. */
static float grav_est[3];

static float clamp(float v, float lim)
{
    if (v > lim)  return lim;
    if (v < -lim) return -lim;
    return v;
}

void har_app_reset(void)
{
    har_pipeline_reset();
    fall_reset(&fall_st);
    for (int i = 0; i < 3; i++) grav_est[i] = 0.0f;
}

void har_app_gravity(float out[3])
{
    for (int i = 0; i < 3; i++) out[i] = grav_est[i];
}

const fall_state_t *har_app_fall_state(void)
{
    return &fall_st;
}

int har_app_on_sample(const float acc_g[3], const float gyro_rads[3],
                      har_app_event_t *ev)
{
    float a[3], g[3];

#if HAR_AXIS_PERMUTE
    /* hapt[i] = SIGN_i * board[SRC_i], ap cho CA acc va gyro. Xem har_app.h de biet
     * cach hieu chuan bo so nay cho tung cach gan board. */
    a[0] = HAR_AXIS_SIGN_X * acc_g[HAR_AXIS_SRC_X];
    a[1] = HAR_AXIS_SIGN_Y * acc_g[HAR_AXIS_SRC_Y];
    a[2] = HAR_AXIS_SIGN_Z * acc_g[HAR_AXIS_SRC_Z];
    g[0] = HAR_AXIS_SIGN_X * gyro_rads[HAR_AXIS_SRC_X];
    g[1] = HAR_AXIS_SIGN_Y * gyro_rads[HAR_AXIS_SRC_Y];
    g[2] = HAR_AXIS_SIGN_Z * gyro_rads[HAR_AXIS_SRC_Z];
#else
    for (int i = 0; i < 3; i++) { a[i] = acc_g[i]; g[i] = gyro_rads[i]; }
#endif

    /* uoc luong gravity de hieu chuan (truoc khi kep, sau khi hoan vi) */
    for (int i = 0; i < 3; i++) grav_est[i] += 0.01f * (a[i] - grav_est[i]);

    /* Kep vao dung dai ma model tung thay khi train. Khong kep thi dac trung
     * max / min / energy se ra ngoai vung do -> model doan bua. */
    for (int i = 0; i < 3; i++) {
        a[i] = clamp(a[i], HAR_CLAMP_ACC_G);
        g[i] = clamp(g[i], HAR_CLAMP_GYRO_RADS);
    }

    har_result_t r;
    if (!har_pipeline_push(a, g, &r)) return 0;

    int e = fall_update(&fall_st, r.label, r.a_min, r.a_max);
    ev->label = r.label;
    ev->fall = (e == FALL_EVENT_FALL);
    ev->recovered = (e == FALL_EVENT_RECOVERED);
    ev->a_min = r.a_min;
    ev->a_max = r.a_max;
    return 1;
}
