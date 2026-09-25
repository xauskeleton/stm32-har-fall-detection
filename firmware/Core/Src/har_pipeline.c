/* Xem har_pipeline.h. */
#include "har_pipeline.h"

#include <math.h>
#include <string.h>

#include "har_classifier.h"
#include "har_dsp.h"
#include "har_features.h"

#define W  HAR_WINDOW

/* Buffer tin hieu: 5 nhom x 3 truc x 128 mau float = 7680 byte.
 * Dung mang phang thay vi ring buffer de har_features() nhan duoc cua so lien tuc;
 * moi lan chay thi dich 64 mau (memmove 3840 byte, ~1 us tren M4F 180 MHz). */
static float buf[HAR_N_SIGNALS][3][W];

/* Kenh phat hien soc: gia toc TONG da loc median 3, KHONG loc lowpass. */
static float shock_buf[W][3];

static har_median3_t med_acc[3], med_gyro[3];
static har_biquad_t  lp_acc[3], lp_gyro[3], grav[3];

static float prev_body[3], prev_gyro[3];   /* de tinh jerk = (x[n] - x[n-1]) * fs */
static int   have_prev;
static int   n_filled;                     /* so mau da co trong buffer, toi da W */
static int   n_since;                      /* so mau moi ke tu lan chay truoc */

static float feat[HAR_N_FEATURES_ALL];

void har_pipeline_reset(void)
{
    memset(buf, 0, sizeof buf);
    memset(shock_buf, 0, sizeof shock_buf);
    for (int ax = 0; ax < 3; ax++) {
        har_median3_reset(&med_acc[ax]);
        har_median3_reset(&med_gyro[ax]);
        har_biquad_reset(&lp_acc[ax]);
        har_biquad_reset(&lp_gyro[ax]);
        har_biquad_reset(&grav[ax]);
    }
    memset(prev_body, 0, sizeof prev_body);
    memset(prev_gyro, 0, sizeof prev_gyro);
    have_prev = 0;
    n_filled = 0;
    n_since = 0;
}

/* Dich toan bo buffer sang trai step mau. */
static void shift_left(int step)
{
    for (int g = 0; g < HAR_N_SIGNALS; g++)
        for (int ax = 0; ax < 3; ax++)
            memmove(buf[g][ax], buf[g][ax] + step, (size_t)(W - step) * sizeof(float));
    memmove(shock_buf, shock_buf + step, (size_t)(W - step) * sizeof(shock_buf[0]));
}

const float *har_pipeline_features(void)
{
    return feat;
}

int har_pipeline_push(const float acc[3], const float gyro[3], har_result_t *out)
{
    float body[3], gyr[3], gv[3], m_acc[3], m_gyro[3];

    /* median 3 tre 1 mau (can biet mau sau) -> mau dau tien chua xuat duoc gi. */
    int ok = 1;
    for (int ax = 0; ax < 3; ax++) {
        ok &= har_median3_push(&med_acc[ax], acc[ax], &m_acc[ax]);
        ok &= har_median3_push(&med_gyro[ax], gyro[ax], &m_gyro[ax]);
    }
    if (!ok) return 0;

    /* Bo mau cu nhat truoc, mau moi se nam o cuoi buffer (chi so W-1). */
    shift_left(1);

    /* --- tien xu ly: lowpass 20 Hz -> tach gravity 0.3 Hz --- */
    for (int ax = 0; ax < 3; ax++) {
        float a = m_acc[ax];
        float w = m_gyro[ax];
        shock_buf[W - 1][ax] = a;          /* kenh soc: median 3, khong lowpass */
        a = har_biquad_push(&lp_acc[ax], HAR_SOS_NOISE, HAR_SOS_NOISE_ZI, a);
        w = har_biquad_push(&lp_gyro[ax], HAR_SOS_NOISE, HAR_SOS_NOISE_ZI, w);
        gv[ax] = har_biquad_push(&grav[ax], HAR_SOS_GRAVITY, HAR_SOS_GRAVITY_ZI, a);
        body[ax] = a - gv[ax];
        gyr[ax] = w;
    }

    /* --- jerk = (x[n] - x[n-1]) * fs. Mau dau tien lay jerk = 0. --- */
    float bj[3], gj[3];
    for (int ax = 0; ax < 3; ax++) {
        bj[ax] = have_prev ? (body[ax] - prev_body[ax]) * (float)HAR_FS : 0.0f;
        gj[ax] = have_prev ? (gyr[ax] - prev_gyro[ax]) * (float)HAR_FS : 0.0f;
        prev_body[ax] = body[ax];
        prev_gyro[ax] = gyr[ax];
    }
    have_prev = 1;

    /* --- day vao cuoi buffer --- */
    for (int ax = 0; ax < 3; ax++) {
        buf[HAR_SIG_BODY_ACC][ax][W - 1] = body[ax];
        buf[HAR_SIG_GRAVITY][ax][W - 1] = gv[ax];
        buf[HAR_SIG_BODY_ACC_JERK][ax][W - 1] = bj[ax];
        buf[HAR_SIG_GYRO][ax][W - 1] = gyr[ax];
        buf[HAR_SIG_GYRO_JERK][ax][W - 1] = gj[ax];
    }

    if (n_filled < W) n_filled++;
    n_since++;

    /* --- du 128 mau va da co HAR_STEP mau moi -> phan loai mot lan --- */
    int ready = 0;
    if (n_filled >= W && n_since >= HAR_STEP) {
        n_since = 0;
        har_features(buf, feat);
        out->label = har_classify(feat, NULL);

        float amin = 1e30f, amax = -1e30f;
        for (int i = 0; i < W; i++) {
            float m = sqrtf(shock_buf[i][0] * shock_buf[i][0]
                            + shock_buf[i][1] * shock_buf[i][1]
                            + shock_buf[i][2] * shock_buf[i][2]);
            if (m < amin) amin = m;
            if (m > amax) amax = m;
        }
        out->a_min = amin;
        out->a_max = amax;
        ready = 1;
    }
    return ready;
}
