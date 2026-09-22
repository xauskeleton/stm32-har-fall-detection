"""
Sinh code C tu model da huan luyen.

Xuat ra:
  firmware/Core/Inc/model_params.h   - hang so + khai bao extern
  firmware/Core/Src/model_params.c   - toan bo so lieu
  firmware/test/golden_vectors.h     - du lieu doi chieu C vs Python

KHONG SUA TAY 3 file tren. Sua o day roi chay lai scripts/export_model.py.

Moi so ghi bang %.9g + hau to 'f' -> float32, dung kieu ma Cortex-M4F dung
(FPU chi co single precision, xem docs/DECISIONS.md muc 10).
"""
import os
from datetime import date

import numpy as np

from . import config as cfg
from . import dsp, fall


def _f(x):
    """float32 literal cho C. '2f' KHONG hop le, phai la '2.0f'."""
    s = '%.9g' % np.float32(x)
    if '.' not in s and 'e' not in s and 'E' not in s:
        s += '.0'
    return s + 'f'


def _arr(name, values, per_line=6, static=False):
    """Mang 1 chieu. static=False -> firmware truy cap qua extern trong header."""
    out = ['%sconst float %s[%d] = {' % ('static ' if static else '', name, len(values))]
    for i in range(0, len(values), per_line):
        out.append('    ' + ' '.join(_f(v) + ',' for v in values[i:i + per_line]))
    return '\n'.join(out + ['};'])


def _arr2(name, mat, per_line=6, static=False):
    """Mang 2 chieu. static=True cho mang nam trong header (tranh multiple definition)."""
    r, c = mat.shape
    out = ['%sconst float %s[%d][%d] = {' % ('static ' if static else '', name, r, c)]
    for i in range(r):
        out.append('    {   /* %d */' % i)
        for j in range(0, c, per_line):
            out.append('        ' + ' '.join(_f(v) + ',' for v in mat[i, j:j + per_line]))
        out.append('    },')
    return '\n'.join(out + ['};'])


_HEADER = '''/* SINH TU DONG boi scripts/export_model.py ngay {date} - KHONG SUA TAY.
 *
 * Model : {arch}, accuracy {acc:.2f}% tren tap test 9 nguoi chua tung thay
 * Nhom  : {groups}
 */
#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

/* ---- Lay mau va cat cua so ---- */
#define HAR_FS               {fs}     /* Hz */
#define HAR_WINDOW           {win}    /* mau = {win_s:.2f} s */
#define HAR_STEP             {step}   /* mau giua 2 lan phan loai = {step_s:.2f} s */
#define HAR_N_SPECTRUM       {half}   /* so bin pho = HAR_WINDOW/2 */

/* ---- Kich thuoc ---- */
#define HAR_N_GROUPS         {ngroup}
#define HAR_N_CHANNELS       {nch}    /* x, y, z, magnitude */
#define HAR_N_FEATURES       {nfeat}
#define HAR_N_CLASSES        {ncls}

/* ---- Thu tu nhom tin hieu: QUYET DINH thu tu dac trung, dao la model sai im lang ---- */
#define HAR_GROUP_TOTAL_ACC  0
#define HAR_GROUP_BODY_ACC   1
#define HAR_GROUP_GYRO       2

/* ---- Loc ---- */
#define HAR_N_SOS            {nsos}   /* section biquad cho lowpass {fc:.1f} Hz tach gravity */
#define HAR_MEDIAN_SHOCK     {med}    /* cua so median, CHI cho kenh phat hien soc */

/* ---- Nguong phat hien te nga ---- */
#define FALL_FREEFALL_G      {ff}
#define FALL_IMPACT_G        {imp}
#define FALL_CONFIRM_WIN     {cw}
#define FALL_LAYING_CONFIRM  {lc}
#define FALL_VOLUNTARY_GUARD {vg}
#define FALL_RECOVER_WIN     {rw}

/* ---- Nhan hoat dong (giu nguyen ID cua HAPT) ---- */
{labels}

/* ---- So lieu (dinh nghia trong model_params.c) ---- */

/* SOS: [section][6] = b0, b1, b2, a0, a1, a2  (a0 luon = 1) */
extern const float HAR_SOS_GRAVITY[HAR_N_SOS][6];

/* Chuan hoa: x_chuan[i] = (x[i] - HAR_SCALER_MEAN[i]) * HAR_SCALER_INV_SCALE[i] */
extern const float HAR_SCALER_MEAN[HAR_N_FEATURES];
extern const float HAR_SCALER_INV_SCALE[HAR_N_FEATURES];

/* Phan loai: score[k] = HAR_BIAS[k] + sum_j HAR_WEIGHTS[k][j] * x_chuan[j], lay argmax */
extern const float HAR_WEIGHTS[HAR_N_CLASSES][HAR_N_FEATURES];
extern const float HAR_BIAS[HAR_N_CLASSES];

/* Ten nhan de in ra UART. label 1..HAR_N_CLASSES, ngoai khoang tra ve "?" */
const char *har_label_name(int label);

#endif /* MODEL_PARAMS_H */
'''

_SOURCE = '''/* SINH TU DONG boi scripts/export_model.py - KHONG SUA TAY. */
#include "model_params.h"

/* Butterworth bac {order}, fc = {fc:.1f} Hz, fs = {fs} Hz, dang second-order sections */
{sos}

/* StandardScaler: luu san 1/scale de ben C chi phai NHAN, khong phai CHIA */
{mean}

{inv}

/* LogisticRegression {ncls} lop x {nfeat} dac trung */
{w}

{b}

static const char *const LABEL_NAMES[HAR_N_CLASSES] = {{
{names}
}};

const char *har_label_name(int label)
{{
    if (label < 1 || label > HAR_N_CLASSES) return "?";
    return LABEL_NAMES[label - 1];
}}
'''

_GOLDEN = '''/* SINH TU DONG boi scripts/export_model.py - KHONG SUA TAY.
 *
 * Doi chieu C voi Python. Nap GOLDEN_ACC/GOLDEN_GYRO vao duong ong streaming,
 * so ket qua voi GOLDEN_FEATURES va GOLDEN_LABELS.
 *
 * NGUONG: sai so TUYET DOI < 1e-3, KHONG dung sai so tuong doi. Ly do:
 * body_acc_*_mean von ~ 0 theo dinh nghia nen sai so tuong doi o do len toi 1.96
 * du sai so tuyet doi chi 1.7e-05. Xem docs/DECISIONS.md muc 10.
 */
#ifndef GOLDEN_VECTORS_H
#define GOLDEN_VECTORS_H

#include "model_params.h"

#define GOLDEN_N_SAMPLES  {n}
#define GOLDEN_N_WINDOWS  {nw}
#define GOLDEN_TOL        1e-3f

{acc}

{gyro}

{feat}

static const int GOLDEN_LABELS[GOLDEN_N_WINDOWS] = {{ {labels} }};

#endif /* GOLDEN_VECTORS_H */
'''


def export(scaler, clf, meta, out_dir='firmware', golden=None, params=None):
    """Sinh file C. golden = (acc, gyro) tin hieu tho; None -> giu nguyen golden_vectors.h.

    Tra ve list duong dan da ghi.
    """
    p = params or fall.FallParams()
    W = np.ascontiguousarray(clf.coef_, dtype=np.float32)
    b = np.ascontiguousarray(clf.intercept_, dtype=np.float32)

    inc, src, tst = (os.path.join(out_dir, *x) for x in
                     (('Core', 'Inc'), ('Core', 'Src'), ('test',)))
    for d in (inc, src, tst):
        os.makedirs(d, exist_ok=True)

    header = _HEADER.format(
        date=date.today().isoformat(), arch=meta['arch'],
        acc=meta['metrics']['accuracy'], groups=', '.join(meta['groups']),
        fs=cfg.FS, win=cfg.WINDOW, win_s=cfg.WINDOW / cfg.FS,
        step=cfg.STEP, step_s=cfg.STEP / cfg.FS, half=cfg.WINDOW // 2,
        ngroup=len(cfg.GROUPS), nch=len(cfg.CHANNELS),
        nfeat=cfg.N_FEATURES, ncls=cfg.N_CLASSES,
        nsos=len(dsp.SOS_GRAV), fc=cfg.FC_GRAV, med=cfg.MEDIAN_SHOCK,
        ff=_f(p.freefall_g), imp=_f(p.impact_g), cw=p.confirm_windows,
        lc=p.laying_confirm, vg=p.voluntary_guard, rw=p.recover_windows,
        labels='\n'.join('#define HAR_%-20s %2d' % (v, k)
                         for k, v in sorted(cfg.ACTIVITY_LABELS.items())))

    source = _SOURCE.format(
        order=cfg.FILTER_ORDER, fc=cfg.FC_GRAV, fs=cfg.FS,
        ncls=cfg.N_CLASSES, nfeat=cfg.N_FEATURES,
        sos=_arr2('HAR_SOS_GRAVITY', dsp.SOS_GRAV.astype(np.float32)),
        mean=_arr('HAR_SCALER_MEAN', scaler.mean_),
        inv=_arr('HAR_SCALER_INV_SCALE', 1.0 / scaler.scale_),
        w=_arr2('HAR_WEIGHTS', W), b=_arr('HAR_BIAS', b),
        names='\n'.join('    "%s",' % cfg.ACTIVITY_LABELS[i]
                        for i in range(1, cfg.N_CLASSES + 1)))

    written = []
    for path, text in ((os.path.join(inc, 'model_params.h'), header),
                       (os.path.join(src, 'model_params.c'), source)):
        with open(path, 'w', newline='\n') as f:
            f.write(text)
        written.append(path)

    if golden is not None:
        acc, gyro = golden
        X = dsp.pipeline(acc, gyro)
        pred = clf.predict(scaler.transform(X))
        path = os.path.join(tst, 'golden_vectors.h')
        with open(path, 'w', newline='\n') as f:
            f.write(_GOLDEN.format(
                n=len(acc), nw=len(X),
                acc=_arr2('GOLDEN_ACC', acc.astype(np.float32), 3, static=True),
                gyro=_arr2('GOLDEN_GYRO', gyro.astype(np.float32), 3, static=True),
                feat=_arr2('GOLDEN_FEATURES', X.astype(np.float32), 5, static=True),
                labels=', '.join(str(int(v)) for v in pred)))
        written.append(path)

    return written
