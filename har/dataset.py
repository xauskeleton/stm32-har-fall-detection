"""
Doc HAPT RawData va cac file do tay (HAPT txt / Phyphox CSV / log UART tu board).
"""
import csv
import os
import re

import numpy as np
from scipy.interpolate import interp1d

from . import config as cfg
from . import dsp

_RE_EXP = re.compile(r'acc_exp(\d+)_user(\d+)\.txt$')


def find_rawdata(path=None):
    """Tim thu muc RawData cua HAPT: tham so -> bien moi truong -> vi tri quen thuoc."""
    candidates = [
        path,
        os.environ.get('HAPT_RAWDATA'),
        'data_raw/RawData',
        'data_raw/smartphone+based+recognition+of+human+activities+and+postural+transitions/RawData',
        '../DSP/data_raw/smartphone+based+recognition+of+human+activities+and+postural+transitions/RawData',
    ]
    for c in candidates:
        if c and os.path.isfile(os.path.join(c, 'labels.txt')):
            return c
    raise FileNotFoundError(
        'Khong tim thay HAPT RawData. Truyen --data <duong/dan/RawData> '
        'hoac dat bien moi truong HAPT_RAWDATA. Xem README muc "Du lieu".'
    )


def load_raw(raw_dir, cache=None, verbose=True):
    """Doc tat ca cap acc/gyro RAW + labels.txt.

    Tra ve (raw, labels) voi raw = dict[(exp,user)] -> {'acc': (N,3), 'gyro': (N,3)}.
    cache = duong dan .npz: lan sau doc nhanh hon nhieu (60 s -> 1 s).
    """
    if cache and os.path.isfile(cache):
        z = np.load(cache, allow_pickle=False)
        raw = {}
        for k in z.files:
            if k == 'labels':
                continue
            kind, exp, user = k.split('_')
            raw.setdefault((int(exp), int(user)), {})[kind] = z[k]
        if verbose:
            print('Da nap %d experiment tu cache %s' % (len(raw), cache))
        return raw, z['labels']

    raw = {}
    for fname in sorted(os.listdir(raw_dir)):
        m = _RE_EXP.match(fname)
        if not m:
            continue
        exp, user = int(m.group(1)), int(m.group(2))
        raw[(exp, user)] = {
            'acc': np.loadtxt(os.path.join(raw_dir, fname)),                        # g
            'gyro': np.loadtxt(os.path.join(raw_dir, fname.replace('acc_', 'gyro_'))),  # rad/s
        }
    if not raw:
        raise FileNotFoundError('Khong co file acc_expXX_userYY.txt trong %s' % raw_dir)

    labels = np.loadtxt(os.path.join(raw_dir, 'labels.txt'), dtype=int)
    if verbose:
        print('Da doc %d experiment tu %s' % (len(raw), raw_dir))
    if cache:
        np.savez_compressed(cache, labels=labels,
                            **{'%s_%d_%d' % (kind, e, u): v[kind]
                               for (e, u), v in raw.items() for kind in ('acc', 'gyro')})
        if verbose:
            print('Da ghi cache %s' % cache)
    return raw, labels


def build_features(raw, labels, verbose=True):
    """Loc + cat cua so theo tung doan co nhan, chia train/test theo NGUOI.

    Tra ve (X_train, y_train, X_test, y_test) voi X shape (n, 120).
    """
    buckets = {'train': [], 'test': []}
    y = {'train': [], 'test': []}

    for exp_id, user_id, act_id, start, end in labels:
        rec = raw.get((exp_id, user_id))
        if rec is None:
            continue
        a, b, g = dsp.preprocess(rec['acc'], rec['gyro'])
        wa = dsp.make_windows(a[start - 1:end])
        if wa is None:
            continue                       # doan ngan hon 1 cua so -> bo
        split = 'train' if user_id in cfg.TRAIN_USERS else 'test'
        buckets[split].append(dsp.extract_features(
            wa, dsp.make_windows(b[start - 1:end]), dsp.make_windows(g[start - 1:end])))
        y[split].append(np.full(len(wa), act_id, dtype=int))

    out = []
    for split in ('train', 'test'):
        out += [np.concatenate(buckets[split]), np.concatenate(y[split])]
    if verbose:
        print('Train: %d cua so | Test: %d cua so' % (len(out[1]), len(out[3])))
    return out[0], out[1], out[2], out[3]


# ---------------------------------------------------------------------------
# File do tay
# ---------------------------------------------------------------------------

def load_hapt_txt(path):
    """File 3 cot, da o 50 Hz. acc don vi g, gyro don vi rad/s."""
    return np.loadtxt(path)


def load_phyphox_csv(path, is_acc=True):
    """CSV Phyphox: cot 0 = thoi gian (s), cot 1..3 = 3 truc. Resample ve 50 Hz.

    is_acc=True -> chia 9.81 doi m/s^2 sang g cho khop don vi HAPT.
    """
    with open(path, 'r', newline='') as f:
        rows = [r for r in csv.reader(f) if len(r) >= 4]
    try:
        float(rows[0][0])
    except ValueError:
        rows = rows[1:]                    # bo dong header
    data = np.array([[float(v) for v in r[:4]] for r in rows])

    t = data[:, 0]
    vals = data[:, 1:4] / 9.81 if is_acc else data[:, 1:4]
    n = max(int((t[-1] - t[0]) * cfg.FS), cfg.WINDOW + 1)
    t_new = np.linspace(t[0], t[-1], n)
    return interp1d(t, vals, axis=0, kind='linear', fill_value='extrapolate')(t_new)


def load_pair(acc_path, gyro_path, fmt='auto'):
    """Doc 1 cap file acc/gyro, tra ve (acc, gyro) da can bang do dai."""
    if fmt == 'auto':
        fmt = 'phyphox' if str(acc_path).lower().endswith('.csv') else 'hapt'
    if fmt == 'phyphox':
        acc = load_phyphox_csv(acc_path, is_acc=True)
        gyro = load_phyphox_csv(gyro_path, is_acc=False)
    else:
        acc, gyro = load_hapt_txt(acc_path), load_hapt_txt(gyro_path)
    n = min(len(acc), len(gyro))
    return acc[:n], gyro[:n]
