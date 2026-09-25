"""
Sinh code C tu model da huan luyen.

Xuat ra:
  firmware/Core/Inc/model_params.h   - hang so + khai bao extern
  firmware/Core/Src/model_params.c   - toan bo so lieu
  firmware/test/golden_vectors.h     - du lieu doi chieu C vs Python

KHONG SUA TAY 3 file tren. Sua o day roi chay lai scripts/export_model.py.

Moi so ghi bang %.9g + hau to 'f' -> float32, dung kieu ma Cortex-M4F dung
(FPU chi co single precision, xem docs/DECISIONS.md muc 6).
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


def _arr_int(name, values, per_line=12, ctype='uint16_t'):
    out = ['const %s %s[%d] = {' % (ctype, name, len(values))]
    for i in range(0, len(values), per_line):
        out.append('    ' + ' '.join('%d,' % v for v in values[i:i + per_line]))
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


def _arr2_int(name, mat, ctype='uint8_t'):
    out = ['const %s %s[%d][%d] = {' % (ctype, name, len(mat), len(mat[0]))]
    for row in mat:
        out.append('    { ' + ' '.join('%d,' % v for v in row) + ' },')
    return '\n'.join(out + ['};'])


_HEADER = '''/* SINH TU DONG boi scripts/export_model.py ngay {date} - KHONG SUA TAY.
 *
 * Model : {arch}, accuracy {acc:.2f}% tren tap test 9 nguoi chua tung thay
 * Nhom  : {signals}
 */
#ifndef MODEL_PARAMS_H
#define MODEL_PARAMS_H

#include <stdint.h>

/* ---- Lay mau va cat cua so ---- */
#define HAR_FS               {fs}     /* Hz */
#define HAR_WINDOW           {win}    /* mau = {win_s:.2f} s */
#define HAR_STEP             {step}   /* mau giua 2 lan phan loai = {step_s:.2f} s */

/* ---- Kich thuoc ---- */
#define HAR_N_SIGNALS        {nsig}   /* nhom tin hieu 3 truc */
#define HAR_N_FEATURES_ALL   {nall}   /* so dac trung TINH RA */
#define HAR_N_FEATURES       {nfeat}  /* so dac trung GIU LAI -> vao model */
#define HAR_N_CLASSES        {ncls}

/* ---- Thu tu nhom tin hieu: QUYET DINH thu tu dac trung, dao la model sai im lang ---- */
{sigdefs}

/* ---- Mien tan so ---- */
#define HAR_N_SPECTRUM       {nspec}  /* bin 1..{nspec} cua FFT {win} diem, BO bin DC */
#define HAR_SPEC_SCALE       {sscale} /* chia pho cho so nay -> bien do 1 phia */
#define HAR_N_BANDS          {nband}  /* so dai tan cho bandsEnergy */

/* ---- Cac phep dac biet ---- */
#define HAR_ENTROPY_BINS     {ebins}  /* histogram o CO DINH tren [0,1] */
#define HAR_AR_ORDER         {arord}  /* bac mo hinh AR (Burg) */

/* ---- Loc: median {med} -> lowpass {fcn:.0f} Hz -> tach gravity {fc:.1f} Hz ---- */
#define HAR_MEDIAN           {med}    /* cua so median, cho CA dac trung VA kenh soc */
#define HAR_N_SOS            {nsos}   /* section biquad, giong nhau cho ca 2 bo loc */

/* ---- Nguong phat hien te nga ---- */
#define FALL_FREEFALL_G      {ff}
#define FALL_IMPACT_G        {imp}
#define FALL_CONFIRM_WIN     {cw}
#define FALL_LAYING_CONFIRM  {lc}
#define FALL_VOLUNTARY_GUARD {vg}
#define FALL_RECOVER_WIN     {rw}
#define FALL_UPRIGHT_MIN     {um}

/* ---- Nhan hoat dong (giu nguyen ID cua HAPT) ---- */
{labels}

/* ---- So lieu (dinh nghia trong model_params.c) ---- */

/* SOS: [section][6] = b0, b1, b2, a0, a1, a2  (a0 luon = 1) */
extern const float HAR_SOS_NOISE[HAR_N_SOS][6];      /* lowpass {fcn:.0f} Hz khu nhieu */
extern const float HAR_SOS_GRAVITY[HAR_N_SOS][6];    /* lowpass {fc:.1f} Hz tach gravity */

/* Trang thai XAC LAP cho dau vao = 1.0. Khoi tao state = ZI[section][i] * mau_dau_tien,
 * nho do khong co transient. Bo qua thi 4 cua so dau sau moi lan cap dien sai nhan. */
extern const float HAR_SOS_NOISE_ZI[HAR_N_SOS][2];
extern const float HAR_SOS_GRAVITY_ZI[HAR_N_SOS][2];

/* Bien cac dai tan cho bandsEnergy: [dai][2] = bin dau, bin cuoi (dem tu 1) */
extern const uint8_t HAR_BANDS[HAR_N_BANDS][2];

/* Chon cot: x_chon[i] = dac_trung[HAR_SELECTED[i]], i = 0..HAR_N_FEATURES-1 */
extern const uint16_t HAR_SELECTED[HAR_N_FEATURES];

/* Chuan hoa: x_chuan[i] = (x_chon[i] - HAR_SCALER_MEAN[i]) * HAR_SCALER_INV_SCALE[i] */
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

/* Butterworth bac {order}, fs = {fs} Hz, dang second-order sections */
{sos_noise}

{zi_noise}

{sos}

{zi_grav}

/* 14 dai tan, so hieu bin dem tu 1 */
{bands}

/* {nfeat} cot duoc RFE chon trong {nall} dac trung tinh ra */
{sel}

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
 * GOLDEN_FEATURES la {nfeat} dac trung DA CHON va CHUA chuan hoa.
 *
 * TIEU CHI CHINH la NHAN: GOLDEN_LABELS phai khop TUNG CUA SO. Da do tren ca 3162
 * cua so tap test: 0 cua so lech giua C (float32) va Python (float64).
 *
 * Nguong dac trung la TUYET DOI, khong dung tuong doi (dac trung mean cua body_acc
 * von ~ 0 theo dinh nghia nen sai so tuong doi o do vo nghia). 0.3 la so do thuc te:
 * sai so lon nhat 0.248 o f_body_acc_x_kurt - mo men bac 4 tren gia tri pho ~17,
 * tuc 1.5% tuong doi. Tuyet dai da so cot sai duoi 1e-4.
 */
#ifndef GOLDEN_VECTORS_H
#define GOLDEN_VECTORS_H

#include "model_params.h"

#define GOLDEN_N_SAMPLES  {n}
#define GOLDEN_N_WINDOWS  {nw}
#define GOLDEN_TOL        0.3f

{acc}

{gyro}

{feat}

static const int GOLDEN_LABELS[GOLDEN_N_WINDOWS] = {{ {labels} }};

#endif /* GOLDEN_VECTORS_H */
'''


def export(scaler, clf, meta, cols, out_dir='firmware', golden=None, params=None):
    """Sinh file C. golden = (acc, gyro) tin hieu tho; None -> giu nguyen golden_vectors.h.

    Tra ve list duong dan da ghi.
    """
    p = params or fall.FallParams()
    W = np.ascontiguousarray(clf.coef_, dtype=np.float32)
    b = np.ascontiguousarray(clf.intercept_, dtype=np.float32)

    dir_inc = os.path.join(out_dir, 'Core', 'Inc')
    dir_src = os.path.join(out_dir, 'Core', 'Src')
    dir_test = os.path.join(out_dir, 'test')
    for d in (dir_inc, dir_src, dir_test):
        os.makedirs(d, exist_ok=True)

    header = _HEADER.format(
        date=date.today().isoformat(), arch=meta['arch'],
        acc=meta['metrics']['accuracy'], signals=', '.join(cfg.SIGNALS),
        fs=cfg.FS, win=cfg.WINDOW, win_s=cfg.WINDOW / cfg.FS,
        step=cfg.STEP, step_s=cfg.STEP / cfg.FS,
        nsig=len(cfg.SIGNALS), nall=cfg.N_FEATURES_ALL, nfeat=len(cols),
        ncls=cfg.N_CLASSES,
        sigdefs='\n'.join('#define HAR_SIG_%-16s %d' % (s.upper(), i)
                          for i, s in enumerate(cfg.SIGNALS)),
        nspec=cfg.N_SPECTRUM, sscale=_f(cfg.SPEC_SCALE), nband=len(cfg.BANDS),
        ebins=cfg.ENTROPY_BINS, arord=cfg.AR_ORDER,
        nsos=len(dsp.SOS_GRAV), fc=cfg.FC_GRAV, fcn=cfg.FC_NOISE, med=cfg.MEDIAN,
        ff=_f(p.freefall_g), imp=_f(p.impact_g), cw=p.confirm_windows,
        lc=p.laying_confirm, vg=p.voluntary_guard, rw=p.recover_windows,
        um=p.upright_min,
        labels='\n'.join('#define HAR_%-20s %2d' % (v, k)
                         for k, v in sorted(cfg.ACTIVITY_LABELS.items())))

    source = _SOURCE.format(
        order=cfg.FILTER_ORDER, fs=cfg.FS,
        ncls=cfg.N_CLASSES, nfeat=len(cols), nall=cfg.N_FEATURES_ALL,
        sos_noise=_arr2('HAR_SOS_NOISE', dsp.SOS_NOISE.astype(np.float32)),
        zi_noise=_arr2('HAR_SOS_NOISE_ZI', dsp.SOS_NOISE_ZI.astype(np.float32)),
        sos=_arr2('HAR_SOS_GRAVITY', dsp.SOS_GRAV.astype(np.float32)),
        zi_grav=_arr2('HAR_SOS_GRAVITY_ZI', dsp.SOS_GRAV_ZI.astype(np.float32)),
        bands=_arr2_int('HAR_BANDS', cfg.BANDS),
        sel=_arr_int('HAR_SELECTED', cols),
        mean=_arr('HAR_SCALER_MEAN', scaler.mean_),
        inv=_arr('HAR_SCALER_INV_SCALE', 1.0 / scaler.scale_),
        w=_arr2('HAR_WEIGHTS', W), b=_arr('HAR_BIAS', b),
        names='\n'.join('    "%s",' % cfg.ACTIVITY_LABELS[i]
                        for i in range(1, cfg.N_CLASSES + 1)))

    written = []
    for path, text in ((os.path.join(dir_inc, 'model_params.h'), header),
                       (os.path.join(dir_src, 'model_params.c'), source)):
        with open(path, 'w', newline='\n') as f:
            f.write(text)
        written.append(path)

    if golden is not None:
        acc, gyro = golden
        X_all = dsp.pipeline(acc, gyro)
        X = X_all[:, cols]
        pred = clf.predict(scaler.transform(X))
        path = os.path.join(dir_test, 'golden_vectors.h')
        with open(path, 'w', newline='\n') as f:
            f.write(_GOLDEN.format(
                n=len(acc), nw=len(X), nfeat=len(cols),
                acc=_arr2('GOLDEN_ACC', acc.astype(np.float32), 3, static=True),
                gyro=_arr2('GOLDEN_GYRO', gyro.astype(np.float32), 3, static=True),
                feat=_arr2('GOLDEN_FEATURES', X.astype(np.float32), 5, static=True),
                labels=', '.join(str(int(v)) for v in pred)))
        written.append(path)

    return written
