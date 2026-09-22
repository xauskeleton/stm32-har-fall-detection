"""
Sinh code C tu model da huan luyen.

Xuat ra:
  firmware/Core/Inc/model_params.h   - khai bao + hang so
  firmware/Core/Src/model_params.c   - toan bo so lieu
  firmware/test/golden_vectors.h     - du lieu doi chieu C vs Python

KHONG SUA TAY 2 file dau. Sua o day roi chay lai scripts/export_model.py.

Moi so deu ghi bang %.9g + hau to 'f' -> float32, dung dinh dang ma
Cortex-M4F dung (FPU chi co single precision, xem docs/DECISIONS.md muc 10).
"""
import os
from datetime import date

import numpy as np

from . import config as cfg
from . import dsp, fall


def _f(x):
    """float32 literal cho C.

    Phai bao dam co dau cham hoac mu: '2f' KHONG phai literal C hop le, '2.0f' moi dung.
    """
    s = '%.9g' % np.float32(x)
    if '.' not in s and 'e' not in s and 'E' not in s:
        s += '.0'
    return s + 'f'


def _arr(name, values, per_line=6, indent='    '):
    """Mang 1 chieu."""
    out = ['static const float %s[%d] = {' % (name, len(values))]
    for i in range(0, len(values), per_line):
        out.append(indent + ' '.join(_f(v) + ',' for v in values[i:i + per_line]))
    out.append('};')
    return '\n'.join(out)


def _arr2(name, mat, per_line=6, indent='        '):
    """Mang 2 chieu."""
    r, c = mat.shape
    out = ['static const float %s[%d][%d] = {' % (name, r, c)]
    for i in range(r):
        out.append('    {   /* %d */' % i)
        for j in range(0, c, per_line):
            out.append(indent + ' '.join(_f(v) + ',' for v in mat[i, j:j + per_line]))
        out.append('    },')
    out.append('};')
    return '\n'.join(out)


def _header(meta, params):
    g = meta['groups']
    n_feat = meta['n_features']
    return '''/* SINH TU DONG boi scripts/export_model.py ngay %s - KHONG SUA TAY.
 *
 * Model : %s
 * Do    : accuracy %.2f%%, macro-F1 tren tap test 9 nguoi chua tung thay
 * Nhom  : %s
 */
#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>

/* ---- Lay mau va cat cua so ---- */
#define HAR_FS              %d      /* Hz */
#define HAR_WINDOW          %d      /* mau = %.2f s */
#define HAR_STEP            %d      /* mau giua 2 lan phan loai = %.2f s */
#define HAR_N_SPECTRUM      %d      /* so bin pho = HAR_WINDOW/2 */

/* ---- Kich thuoc ---- */
#define HAR_N_GROUPS        %d
#define HAR_N_CHANNELS      %d      /* x, y, z, magnitude */
#define HAR_N_FEAT_TIME     %d
#define HAR_N_FEAT_FREQ     %d
#define HAR_N_FEATURES      %d
#define HAR_N_CLASSES       %d

/* ---- Nhom tin hieu (thu tu QUYET DINH thu tu dac trung) ---- */
#define HAR_GROUP_TOTAL_ACC 0
#define HAR_GROUP_BODY_ACC  1
#define HAR_GROUP_GYRO      2

/* ---- Loc tach gravity: Butterworth bac 3, fc = %.1f Hz, dang SOS ---- */
#define HAR_N_SOS           %d      /* so section biquad */

/* ---- Loc median cho kenh phat hien soc ---- */
#define HAR_MEDIAN_SHOCK    %d

/* ---- Nguong phat hien te nga ---- */
#define FALL_FREEFALL_G     %s
#define FALL_IMPACT_G       %s
#define FALL_CONFIRM_WIN    %d
#define FALL_LAYING_CONFIRM %d
#define FALL_VOLUNTARY_GUARD %d
#define FALL_RECOVER_WIN    %d

/* ---- Nhan hoat dong (giu nguyen ID cua HAPT) ---- */
%s

/* ---- Ham truy cap so lieu (dinh nghia trong model_params.c) ---- */

/* He so SOS: [section][6] = {b0, b1, b2, a0, a1, a2}, a0 luon = 1 */
const float *har_sos_gravity(int section);

/* Chuan hoa: x_chuan = (x - mean[i]) * inv_scale[i] */
const float *har_scaler_mean(void);
const float *har_scaler_inv_scale(void);

/* Phan loai tuyen tinh: score[k] = bias[k] + sum_j W[k][j] * x_chuan[j] */
const float *har_weights(int class_idx);
const float *har_bias(void);

/* Ten nhan de in ra UART */
const char *har_label_name(int label);

#endif /* MODEL_PARAMS_H */
''' % (
        date.today().isoformat(), meta['arch'], meta['metrics']['accuracy'],
        ', '.join(g),
        cfg.FS, cfg.WINDOW, cfg.WINDOW / cfg.FS, cfg.STEP, cfg.STEP / cfg.FS,
        cfg.WINDOW // 2,
        len(g), len(cfg.CHANNELS), len(cfg.TIME_FEATURES), len(cfg.FREQ_FEATURES),
        n_feat, cfg.N_CLASSES,
        cfg.FC_GRAV, len(dsp.SOS_GRAV), cfg.MEDIAN_SHOCK,
        _f(params.freefall_g), _f(params.impact_g),
        params.confirm_windows, params.laying_confirm,
        params.voluntary_guard, params.recover_windows,
        '\n'.join('#define HAR_%-20s %2d' % (v, k)
                  for k, v in sorted(cfg.ACTIVITY_LABELS.items())),
    )


def _source(scaler, W, b):
    inv_scale = (1.0 / scaler.scale_).astype(np.float32)
    names = '\n'.join('    "%s",' % cfg.ACTIVITY_LABELS[i]
                      for i in range(1, cfg.N_CLASSES + 1))
    return '''/* SINH TU DONG boi scripts/export_model.py - KHONG SUA TAY. */
#include "model_params.h"

/* Butterworth bac 3, fc = %.1f Hz, fs = %d Hz, dang second-order sections.
 * Moi hang: b0, b1, b2, a0, a1, a2  (a0 = 1) */
%s

/* StandardScaler: luu san 1/scale de ben C chi phai NHAN, khong phai CHIA */
%s

%s

/* LogisticRegression: %d lop x %d dac trung */
%s

%s

static const char *const LABEL_NAMES[%d] = {
%s
};

const float *har_sos_gravity(int section) { return SOS_GRAVITY[section]; }
const float *har_scaler_mean(void)        { return SCALER_MEAN; }
const float *har_scaler_inv_scale(void)   { return SCALER_INV_SCALE; }
const float *har_weights(int class_idx)   { return WEIGHTS[class_idx]; }
const float *har_bias(void)               { return BIAS; }

const char *har_label_name(int label)
{
    if (label < 1 || label > HAR_N_CLASSES) return "?";
    return LABEL_NAMES[label - 1];
}
''' % (
        cfg.FC_GRAV, cfg.FS,
        _arr2('SOS_GRAVITY', dsp.SOS_GRAV.astype(np.float32), per_line=6),
        _arr('SCALER_MEAN', scaler.mean_),
        _arr('SCALER_INV_SCALE', inv_scale),
        W.shape[0], W.shape[1],
        _arr2('WEIGHTS', W),
        _arr('BIAS', b),
        cfg.N_CLASSES, names,
    )


def _golden(acc, gyro, X, labels, n_sample, synthetic=False):
    """Du lieu doi chieu: tin hieu tho vao -> dac trung + nhan mong doi."""
    n_win = len(X)
    warn = ('''
 * CANH BAO: du lieu nay sinh tu TIN HIEU TONG HOP (song sin), khong phai do that.
 * Test van dung duoc (tu nhat quan) nhung yeu hon: song sin khong phu het dai gia
 * tri cua du lieu that. Nen sinh lai khi co HAPT RawData hoac file do tay.
 *''' if synthetic else '')
    return '''/* SINH TU DONG boi scripts/export_model.py - KHONG SUA TAY.
 *%s
 * Doi chieu C voi Python. Nap GOLDEN_ACC/GOLDEN_GYRO vao duong ong streaming,
 * so ket qua voi GOLDEN_FEATURES va GOLDEN_LABELS.
 *
 * NGUONG: dung sai so TUYET DOI < 1e-3, KHONG dung sai so tuong doi.
 * Ly do: body_acc_*_mean von ~ 0 theo dinh nghia, sai so tuong doi o do len toi
 * 1.96 du sai so tuyet doi chi 1.7e-05. Xem docs/DECISIONS.md muc 10.
 */
#ifndef GOLDEN_VECTORS_H
#define GOLDEN_VECTORS_H

#include "model_params.h"

#define GOLDEN_N_SAMPLES  %d
#define GOLDEN_N_WINDOWS  %d
#define GOLDEN_TOL        1e-3f

%s

%s

%s

static const int GOLDEN_LABELS[%d] = { %s };

#endif /* GOLDEN_VECTORS_H */
''' % (
        warn, n_sample, n_win,
        _arr2('GOLDEN_ACC', acc.astype(np.float32), per_line=3),
        _arr2('GOLDEN_GYRO', gyro.astype(np.float32), per_line=3),
        _arr2('GOLDEN_FEATURES', X.astype(np.float32), per_line=5),
        n_win, ', '.join(str(int(v)) for v in labels),
    )


def export(scaler, clf, meta, out_dir='firmware', golden=None, params=None,
           golden_synthetic=False):
    """Sinh toan bo file C.

    golden = (acc, gyro) tin hieu tho de tao vector doi chieu. None -> khong ghi
    golden_vectors.h (giu nguyen file da co).
    """
    params = params or fall.FallParams()
    W = np.ascontiguousarray(clf.coef_, dtype=np.float32)
    b = np.ascontiguousarray(clf.intercept_, dtype=np.float32)

    inc = os.path.join(out_dir, 'Core', 'Inc')
    src = os.path.join(out_dir, 'Core', 'Src')
    tst = os.path.join(out_dir, 'test')
    for d in (inc, src, tst):
        os.makedirs(d, exist_ok=True)

    written = []
    for path, text in ((os.path.join(inc, 'model_params.h'), _header(meta, params)),
                       (os.path.join(src, 'model_params.c'), _source(scaler, W, b))):
        with open(path, 'w', newline='\n') as f:
            f.write(text)
        written.append(path)

    if golden is not None:
        acc, gyro = golden
        X = dsp.pipeline(acc, gyro)
        pred = clf.predict(scaler.transform(X))
        path = os.path.join(tst, 'golden_vectors.h')
        with open(path, 'w', newline='\n') as f:
            f.write(_golden(acc, gyro, X, pred, len(acc)))
        written.append(path)

    return written
