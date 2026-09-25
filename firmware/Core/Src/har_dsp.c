/* Xem har_dsp.h. Ban tham chieu Python: har/dsp.py. */
#include "har_dsp.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define W      HAR_WINDOW
#define PI_F   3.14159265358979f

/* ---------------------------------------------------------------- median 3 */

void har_median3_reset(har_median3_t *st)
{
    st->a = 0.0f;
    st->b = 0.0f;
    st->n = 0;
}

int har_median3_push(har_median3_t *st, float x, float *out)
{
    int ready = 1;
    if (st->n == 0) {
        ready = 0;                     /* chua co mau nao de xuat */
    } else if (st->n == 1) {
        *out = st->b;                  /* chi so 0 = x[0], giu nguyen nhu Python */
    } else {
        float a = st->a, b = st->b, c = x;    /* 3 mau lien tiep */
        if ((a <= b && b <= c) || (c <= b && b <= a))      *out = b;
        else if ((b <= a && a <= c) || (c <= a && a <= b))  *out = a;
        else                                               *out = c;
    }
    st->a = st->b;
    st->b = x;
    if (st->n < 2) st->n++;
    return ready;
}

/* ------------------------------------------------------------ biquad SOS */

void har_biquad_reset(har_biquad_t *st)
{
    memset(st->z, 0, sizeof st->z);
    st->primed = 0;
}

float har_biquad_push(har_biquad_t *st, const float sos[HAR_N_SOS][6],
                      const float zi[HAR_N_SOS][2], float x)
{
    if (!st->primed) {
        /* zi la trang thai xac lap cho dau vao = 1.0 -> nhan voi mau dau tien */
        for (int s = 0; s < HAR_N_SOS; s++) {
            st->z[s][0] = zi[s][0] * x;
            st->z[s][1] = zi[s][1] * x;
        }
        st->primed = 1;
    }
    float v = x;
    for (int s = 0; s < HAR_N_SOS; s++) {
        float y = sos[s][0] * v + st->z[s][0];
        st->z[s][0] = sos[s][1] * v - sos[s][4] * y + st->z[s][1];
        st->z[s][1] = sos[s][2] * v - sos[s][5] * y;
        v = y;
    }
    return v;
}

/* ---------------------------------------------------------------- pho FFT */

static float fft_re[W], fft_im[W];

void har_spectrum(const float *x, float *out)
{
    for (int i = 0; i < W; i++) { fft_re[i] = x[i]; fft_im[i] = 0.0f; }

    for (int i = 1, j = 0; i < W; i++) {          /* dao bit */
        int b = W >> 1;
        for (; j & b; b >>= 1) j ^= b;
        j ^= b;
        if (i < j) {
            float t = fft_re[i]; fft_re[i] = fft_re[j]; fft_re[j] = t;
            t = fft_im[i]; fft_im[i] = fft_im[j]; fft_im[j] = t;
        }
    }
    for (int len = 2; len <= W; len <<= 1) {
        float ang = -2.0f * PI_F / (float)len;
        float wr = cosf(ang), wi = sinf(ang);
        for (int i = 0; i < W; i += len) {
            float cr = 1.0f, ci = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                int p = i + k, q = p + len / 2;
                float tr = fft_re[q] * cr - fft_im[q] * ci;
                float ti = fft_re[q] * ci + fft_im[q] * cr;
                fft_re[q] = fft_re[p] - tr; fft_im[q] = fft_im[p] - ti;
                fft_re[p] += tr;            fft_im[p] += ti;
                float nr = cr * wr - ci * wi;
                ci = cr * wi + ci * wr;
                cr = nr;
            }
        }
    }
    for (int k = 0; k < HAR_N_SPECTRUM; k++) {
        int i = k + 1;                            /* bo bin DC */
        out[k] = sqrtf(fft_re[i] * fft_re[i] + fft_im[i] * fft_im[i]) / HAR_SPEC_SCALE;
    }
}

/* ------------------------------------------------------- thong ke co ban */

float har_mean(const float *x, int n)
{
    float s = 0.0f;
    for (int i = 0; i < n; i++) s += x[i];
    return s / (float)n;
}

float har_std(const float *x, int n)
{
    float m = har_mean(x, n), s = 0.0f;
    for (int i = 0; i < n; i++) { float d = x[i] - m; s += d * d; }
    return sqrtf(s / (float)n);
}

float har_mad(const float *x, int n)
{
    float m = har_mean(x, n), s = 0.0f;
    for (int i = 0; i < n; i++) s += fabsf(x[i] - m);
    return s / (float)n;
}

float har_max(const float *x, int n)
{
    float v = x[0];
    for (int i = 1; i < n; i++) if (x[i] > v) v = x[i];
    return v;
}

float har_min(const float *x, int n)
{
    float v = x[0];
    for (int i = 1; i < n; i++) if (x[i] < v) v = x[i];
    return v;
}

float har_energy(const float *x, int n)
{
    float s = 0.0f;
    for (int i = 0; i < n; i++) s += x[i] * x[i];
    return s / (float)n;
}

static int cmp_f(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

/* Quy uoc MATLAB (i-0.5)/n = numpy method='midpoint': vi tri q*(n-1), khong nguyen
 * thi lay trung binh 2 phan tu ke nhau. Voi n = 128: 31.75 -> (s[31]+s[32])/2. */
float har_iqr(const float *x, int n)
{
    static float srt[W];
    memcpy(srt, x, (size_t)n * sizeof(float));
    qsort(srt, (size_t)n, sizeof(float), cmp_f);

    float i25 = 0.25f * (float)(n - 1), i75 = 0.75f * (float)(n - 1);
    int l25 = (int)floorf(i25), l75 = (int)floorf(i75);
    float p25 = (i25 == (float)l25) ? srt[l25] : 0.5f * (srt[l25] + srt[l25 + 1]);
    float p75 = (i75 == (float)l75) ? srt[l75] : 0.5f * (srt[l75] + srt[l75 + 1]);
    return p75 - p25;
}

/* entropy() cua MATLAB Image Processing Toolbox: histogram HAR_ENTROPY_BINS o CO DINH
 * tren [0,1], o = round(x*255), gia tri ngoai bi ep ve bien. O co dinh nen phep nay
 * KHONG bat bien theo ti le - do la co y, va la ly do pho phai chia HAR_SPEC_SCALE. */
float har_entropy(const float *x, int n)
{
    static int cnt[HAR_ENTROPY_BINS];
    memset(cnt, 0, sizeof cnt);
    for (int i = 0; i < n; i++) {
        int b = (int)nearbyintf(x[i] * (float)(HAR_ENTROPY_BINS - 1));
        if (b < 0) b = 0;
        if (b > HAR_ENTROPY_BINS - 1) b = HAR_ENTROPY_BINS - 1;
        cnt[b]++;
    }
    float e = 0.0f;
    for (int b = 0; b < HAR_ENTROPY_BINS; b++) {
        if (cnt[b] == 0) continue;
        float p = (float)cnt[b] / (float)n;
        e -= p * log2f(p);
    }
    return e;
}

float har_skew(const float *x, int n)
{
    float m = har_mean(x, n), sd = har_std(x, n) + 1e-30f, s = 0.0f;
    for (int i = 0; i < n; i++) { float d = x[i] - m; s += d * d * d; }
    return (s / (float)n) / (sd * sd * sd);
}

float har_kurt(const float *x, int n)
{
    float m = har_mean(x, n), sd = har_std(x, n) + 1e-30f, s = 0.0f;
    for (int i = 0; i < n; i++) { float d = x[i] - m, d2 = d * d; s += d2 * d2; }
    return (s / (float)n) / (sd * sd * sd * sd);
}

float har_corr(const float *x, const float *y, int n)
{
    float mx = har_mean(x, n), my = har_mean(y, n);
    float sxx = 0.0f, syy = 0.0f, sxy = 0.0f;
    for (int i = 0; i < n; i++) {
        float a = x[i] - mx, b = y[i] - my;
        sxx += a * a; syy += b * b; sxy += a * b;
    }
    return sxy / sqrtf(sxx * syy + 1e-30f);
}

float har_mean_freq(const float *x, int n)
{
    float num = 0.0f, den = 0.0f;
    for (int i = 0; i < n; i++) {
        float f = (float)(i + 1) * (float)HAR_FS / (float)W;   /* bin 1..64 */
        num += x[i] * f;
        den += x[i];
    }
    return num / (den + 1e-30f);
}

/* He so mo hinh AR bac HAR_AR_ORDER bang thuat toan Burg. Mo hinh AR mo ta tin hieu
 * bang chinh no o cac mau truoc:  x[n] = -(a1*x[n-1] + ... + a4*x[n-4]) + nhieu.
 * Burg tim a1..a4 sao cho tong sai so tien va sai so lui nho nhat.
 *
 * KHONG goi phep nay tren tin hieu gan HANG SO (nhu gravity): he so hoi tu ve (1-z)^4
 * nen (1 - mu^2) triet tieu het chu so co nghia cua float32. Xem DECISIONS.md muc 7. */
void har_burg(const float *x, int n, float *out)
{
    static float fwd[W], bwd[W];
    float a[HAR_AR_ORDER + 1], an[HAR_AR_ORDER + 1];

    memcpy(fwd, x, (size_t)n * sizeof(float));
    memcpy(bwd, x, (size_t)n * sizeof(float));
    memset(a, 0, sizeof a);
    a[0] = 1.0f;

    float den = 0.0f;
    for (int i = 0; i < n; i++) den += 2.0f * x[i] * x[i];
    den -= x[0] * x[0] + x[n - 1] * x[n - 1];

    for (int k = 0; k < HAR_AR_ORDER; k++) {
        float num = 0.0f;
        for (int i = 0; i < n - k - 1; i++) num += fwd[i + k + 1] * bwd[i];
        float mu = (den != 0.0f) ? 2.0f * num / den : 0.0f;

        memcpy(an, a, sizeof a);
        for (int m = 1; m <= k + 1; m++) an[m] = a[m] - mu * a[k + 1 - m];
        memcpy(a, an, sizeof a);

        for (int i = n - 1; i > k; i--) {
            float ft = fwd[i] - mu * bwd[i - k - 1];
            bwd[i - k - 1] = bwd[i - k - 1] - mu * fwd[i];
            fwd[i] = ft;
        }
        den = (1.0f - mu * mu) * den - fwd[k + 1] * fwd[k + 1]
              - bwd[n - k - 2] * bwd[n - k - 2];
    }
    for (int i = 0; i < HAR_AR_ORDER; i++) out[i] = a[i + 1];
}

float har_angle(const float *v, const float *g)
{
    float nv = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) + 1e-30f;
    float ng = sqrtf(g[0] * g[0] + g[1] * g[1] + g[2] * g[2]) + 1e-30f;
    float d = (v[0] * g[0] + v[1] * g[1] + v[2] * g[2]) / (nv * ng);
    if (d > 1.0f)  d = 1.0f;
    if (d < -1.0f) d = -1.0f;
    return acosf(d) - PI_F / 2.0f;
}
