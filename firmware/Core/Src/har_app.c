/* Xem har_app.h. */
#include "har_app.h"

#include "har_pipeline.h"

static fall_state_t fall_st;

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
    /* (x, y, z)_hapt = (-x, +z, +y)_board, ap cho CA acc va gyro */
    a[0] = -acc_g[0];  a[1] = acc_g[2];  a[2] = acc_g[1];
    g[0] = -gyro_rads[0];  g[1] = gyro_rads[2];  g[2] = gyro_rads[1];
#else
    for (int i = 0; i < 3; i++) { a[i] = acc_g[i]; g[i] = gyro_rads[i]; }
#endif

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
