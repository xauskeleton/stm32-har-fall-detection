#include "har_dsp.h"
#include "arm_math.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

static IMU_Sample_t ring_buffer[DSP_WINDOW];
static uint16_t ring_head = 0;
static uint32_t total_samples = 0;
static uint16_t new_sample_count = 0;

static arm_rfft_fast_instance_f32 fft_inst;

// Butterworth Lowpass 0.3Hz @ 50Hz (Order 3 -> 2 Biquad stages)
// CMSIS-DSP DF1 Coeffs format: {b0, b1, b2, a1, a2} where a1 = -a1_scipy, a2 = -a2_scipy
static float32_t biquad_coeffs_grav[10] = {
    6.45184913e-06f, 1.29036983e-05f, 6.45184913e-06f, 9.62994051e-01f, -0.00000000e+00f,
    1.00000000e+00f, 1.00000000e+00f, 0.00000000e+00f, 1.96161218e+00f, -9.63006955e-01f
};
static float32_t biquad_state_grav_x[8];
static float32_t biquad_state_grav_y[8];
static float32_t biquad_state_grav_z[8];
static arm_biquad_casd_df1_inst_f32 S_grav_x;
static arm_biquad_casd_df1_inst_f32 S_grav_y;
static arm_biquad_casd_df1_inst_f32 S_grav_z;

// Helper: Bubble/Insertion sort for median calculation
static void sort_float(float *arr, int n) {
    for (int i = 1; i < n; i++) {
        float key = arr[i];
        int j = i - 1;
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j--;
        }
        arr[j + 1] = key;
    }
}

static float calc_median(const float *arr, int n) {
    static float temp[DSP_WINDOW];
    memcpy(temp, arr, n * sizeof(float));
    sort_float(temp, n);
    if (n % 2 == 0) {
        return (temp[n / 2 - 1] + temp[n / 2]) * 0.5f;
    } else {
        return temp[n / 2];
    }
}

static float calc_mad(const float *arr, int n) {
    float med = calc_median(arr, n);
    static float diffs[DSP_WINDOW];
    for (int i = 0; i < n; i++) {
        diffs[i] = fabsf(arr[i] - med);
    }
    return calc_median(diffs, n);
}

void DSP_Init(void) {
    ring_head = 0;
    total_samples = 0;
    new_sample_count = 0;
    memset(ring_buffer, 0, sizeof(ring_buffer));

    arm_rfft_fast_init_f32(&fft_inst, DSP_WINDOW);

    arm_biquad_cascade_df1_init_f32(&S_grav_x, 2, biquad_coeffs_grav, biquad_state_grav_x);
    arm_biquad_cascade_df1_init_f32(&S_grav_y, 2, biquad_coeffs_grav, biquad_state_grav_y);
    arm_biquad_cascade_df1_init_f32(&S_grav_z, 2, biquad_coeffs_grav, biquad_state_grav_z);
}

bool DSP_PushSample(const IMU_Sample_t *sample) {
    ring_buffer[ring_head] = *sample;
    ring_head = (ring_head + 1) % DSP_WINDOW;
    total_samples++;
    new_sample_count++;

    if (total_samples >= DSP_WINDOW && new_sample_count >= DSP_STEP) {
        new_sample_count = 0;
        return true;
    }
    return false;
}

float DSP_GetLatestAccMag(void) {
    uint16_t latest_idx = (ring_head == 0) ? (DSP_WINDOW - 1) : (ring_head - 1);
    IMU_Sample_t *s = &ring_buffer[latest_idx];
    return sqrtf(s->ax * s->ax + s->ay * s->ay + s->az * s->az);
}

static void extract_channel_features(const float *s, float *out_feats) {
    // 1. Mean
    arm_mean_f32((float32_t*)s, DSP_WINDOW, &out_feats[0]);
    // 2. Std
    arm_std_f32((float32_t*)s, DSP_WINDOW, &out_feats[1]);
    // 3. Max
    arm_max_f32((float32_t*)s, DSP_WINDOW, &out_feats[2], &(uint32_t){0});
    // 4. Min
    arm_min_f32((float32_t*)s, DSP_WINDOW, &out_feats[3], &(uint32_t){0});
    // 5. RMS
    arm_rms_f32((float32_t*)s, DSP_WINDOW, &out_feats[4]);
    // 6. MAD
    out_feats[5] = calc_mad(s, DSP_WINDOW);

    // 7-10: Frequency Domain (FFT 128)
    static float fft_in[DSP_WINDOW];
    static float fft_out[DSP_WINDOW];
    memcpy(fft_in, s, sizeof(fft_in));

    arm_rfft_fast_f32(&fft_inst, fft_in, fft_out, 0);

    // Compute magnitude spectrum for first 64 bins
    static float mag_fft[DSP_WINDOW / 2];
    mag_fft[0] = fabsf(fft_out[0]); // DC
    mag_fft[DSP_WINDOW / 2 - 1] = fabsf(fft_out[1]); // Nyquist is stored in fft_out[1] by CMSIS fast rfft
    for (int k = 1; k < DSP_WINDOW / 2 - 1; k++) {
        float re = fft_out[2 * k];
        float im = fft_out[2 * k + 1];
        mag_fft[k] = sqrtf(re * re + im * im);
    }

    // 7. FFT Max
    arm_max_f32(mag_fft, DSP_WINDOW / 2, &out_feats[6], &(uint32_t){0});
    // 8. FFT Mean
    arm_mean_f32(mag_fft, DSP_WINDOW / 2, &out_feats[7]);
    // 9. FFT Energy (mean of squared magnitude)
    float sum_sq = 0.0f;
    for (int k = 0; k < DSP_WINDOW / 2; k++) {
        sum_sq += mag_fft[k] * mag_fft[k];
    }
    out_feats[8] = sum_sq / (DSP_WINDOW / 2.0f);

    // 10. Mean Frequency (Spectral Centroid)
    float weighted_sum = 0.0f;
    float total_mag = 0.0f;
    float freq_resolution = (float)DSP_FS / (float)DSP_WINDOW; // 50 / 128
    for (int k = 0; k < DSP_WINDOW / 2; k++) {
        float freq = k * freq_resolution;
        weighted_sum += freq * mag_fft[k];
        total_mag += mag_fft[k];
    }
    out_feats[9] = weighted_sum / (total_mag + 1e-10f);
}

void DSP_ExtractFeatures(float *out_features) {
    static float tot_acc_x[DSP_WINDOW], tot_acc_y[DSP_WINDOW], tot_acc_z[DSP_WINDOW];
    static float body_acc_x[DSP_WINDOW], body_acc_y[DSP_WINDOW], body_acc_z[DSP_WINDOW];
    static float gyro_x[DSP_WINDOW], gyro_y[DSP_WINDOW], gyro_z[DSP_WINDOW];

    // Read continuous 128 samples from ring buffer
    uint16_t start_idx = ring_head; // oldest sample
    for (int i = 0; i < DSP_WINDOW; i++) {
        uint16_t idx = (start_idx + i) % DSP_WINDOW;
        tot_acc_x[i] = ring_buffer[idx].ax;
        tot_acc_y[i] = ring_buffer[idx].ay;
        tot_acc_z[i] = ring_buffer[idx].az;
        gyro_x[i]    = ring_buffer[idx].gx;
        gyro_y[i]    = ring_buffer[idx].gy;
        gyro_z[i]    = ring_buffer[idx].gz;
    }

    // Filter total_acc to get gravity and body_acc
    static float grav_x[DSP_WINDOW], grav_y[DSP_WINDOW], grav_z[DSP_WINDOW];
    arm_biquad_cascade_df1_f32(&S_grav_x, tot_acc_x, grav_x, DSP_WINDOW);
    arm_biquad_cascade_df1_f32(&S_grav_y, tot_acc_y, grav_y, DSP_WINDOW);
    arm_biquad_cascade_df1_f32(&S_grav_z, tot_acc_z, grav_z, DSP_WINDOW);

    for (int i = 0; i < DSP_WINDOW; i++) {
        body_acc_x[i] = tot_acc_x[i] - grav_x[i];
        body_acc_y[i] = tot_acc_y[i] - grav_y[i];
        body_acc_z[i] = tot_acc_z[i] - grav_z[i];
    }

    int feat_idx = 0;

    // Groups: total_acc, body_acc, gyro
    float *group_data[3][3] = {
        {tot_acc_x, tot_acc_y, tot_acc_z},
        {body_acc_x, body_acc_y, body_acc_z},
        {gyro_x, gyro_y, gyro_z}
    };

    for (int g = 0; g < 3; g++) {
        float *x_arr = group_data[g][0];
        float *y_arr = group_data[g][1];
        float *z_arr = group_data[g][2];
        static float mag_arr[DSP_WINDOW];

        for (int i = 0; i < DSP_WINDOW; i++) {
            mag_arr[i] = sqrtf(x_arr[i] * x_arr[i] + y_arr[i] * y_arr[i] + z_arr[i] * z_arr[i]);
        }

        float *channels[4] = {x_arr, y_arr, z_arr, mag_arr};
        for (int ch = 0; ch < 4; ch++) {
            extract_channel_features(channels[ch], &out_features[feat_idx]);
            feat_idx += 10;
        }
    }
}
