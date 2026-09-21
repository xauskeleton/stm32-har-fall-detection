"""
Doc HAPT RawData va cac file do tay (HAPT txt / Phyphox CSV / log UART tu STM32).
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
    """Tim thu muc RawData cua HAPT. Uu tien tham so, roi bien moi truong, roi cac vi tri quen thuoc."""
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
        'hoac set bien moi truong HAPT_RAWDATA. Xem README muc "Du lieu".'
    )


def load_raw(raw_dir, cache=None, verbose=True):
    """Doc tat ca cap acc/gyro RAW (chua loc) + labels.txt.

    Tach rieng khoi buoc loc de sweep nhieu cau hinh ma chi doc dia MOT lan.
    Tra ve (raw, labels) voi raw = dict[(exp,user)] -> {'acc': (N,3), 'gyro': (N,3)}.
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
            print(f'Da nap {len(raw)} experiment tu cache {cache}')
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
        raise FileNotFoundError(f'Khong co file acc_expXX_userYY.txt trong {raw_dir}')

    labels = np.loadtxt(os.path.join(raw_dir, 'labels.txt'), dtype=int)
    if verbose:
        print(f'Da doc {len(raw)} experiment tu {raw_dir}')
    if cache:
        np.savez_compressed(cache, labels=labels,
                            **{f'{kind}_{e}_{u}': v[kind]
                               for (e, u), v in raw.items() for kind in ('acc', 'gyro')})
        if verbose:
            print(f'Da ghi cache {cache}')
    return raw, labels


def build_windows(raw, labels, mode='causal', use_butter=cfg.USE_BUTTER,
                  groups=cfg.GROUPS_FULL, verbose=True):
    """Loc + cat cua so theo tung doan co nhan, chia train/test theo subject.

    Tra ve dict: {'train': {nhom: (n,128,3)}, 'test': {...}, 'y_train': .., 'y_test': ..}
    """
    sigs = {k: dsp.preprocess(v['acc'], v['gyro'], mode=mode,
                              use_butter=use_butter, groups=groups)
            for k, v in raw.items()}

    buckets = {'train': {g: [] for g in groups}, 'test': {g: [] for g in groups}}
    y = {'train': [], 'test': []}

    for exp_id, user_id, act_id, start, end in labels:
        rec = sigs.get((exp_id, user_id))
        if rec is None:
            continue
        split = 'train' if user_id in cfg.TRAIN_USERS else 'test'
        wins = {g: dsp.make_windows(rec[g][start - 1:end]) for g in groups}
        if wins[groups[0]] is None:
            continue                       # doan ngan hon 1 cua so -> bo
        for g in groups:
            buckets[split][g].append(wins[g])
        y[split].append(np.full(len(wins[groups[0]]), act_id, dtype=int))

    result = {}
    for split in ('train', 'test'):
        result[split] = {g: np.concatenate(v) for g, v in buckets[split].items()}
        result[f'y_{split}'] = np.concatenate(y[split])
    if verbose:
        print(f"Train: {len(result['y_train'])} cua so | Test: {len(result['y_test'])} cua so")
    return result


def build_features(raw, labels, mode='causal', use_butter=cfg.USE_BUTTER,
                   groups=cfg.GROUPS_FULL, use_fft=True, verbose=False):
    """Tat ca trong mot buoc -> (X_train, y_train, X_test, y_test)."""
    w = build_windows(raw, labels, mode=mode, use_butter=use_butter,
                      groups=groups, verbose=verbose)
    X_tr = dsp.extract_features(w['train'], groups=groups, use_fft=use_fft)
    X_te = dsp.extract_features(w['test'], groups=groups, use_fft=use_fft)
    return X_tr, w['y_train'], X_te, w['y_test']


# ---------------------------------------------------------------------------
# File do tay
# ---------------------------------------------------------------------------

def load_hapt_txt(path):
    """File 3 cot, da o 50Hz. acc don vi g, gyro don vi rad/s."""
    return np.loadtxt(path)


def load_phyphox_csv(path, is_acc=True):
    """CSV Phyphox: cot 0 = thoi gian (s), cot 1..3 = 3 truc. Resample tuyen tinh ve 50Hz.

    is_acc=True -> chia 9.81 doi m/s^2 sang g cho khop don vi HAPT.
    """
    with open(path, 'r', newline='') as f:
        rows = [r for r in csv.reader(f) if len(r) >= 4]
    # bo dong header neu cot dau khong parse duoc thanh so
    try:
        float(rows[0][0])
    except ValueError:
        rows = rows[1:]
    data = np.array([[float(v) for v in r[:4]] for r in rows])

    t = data[:, 0]
    vals = data[:, 1:4] / 9.81 if is_acc else data[:, 1:4]
    n = max(int((t[-1] - t[0]) * cfg.FS), cfg.WINDOW + 1)
    t_new = np.linspace(t[0], t[-1], n)
    return interp1d(t, vals, axis=0, kind='linear', fill_value='extrapolate')(t_new)


def load_uart_log(path):
    """Log CSV do tools/serial_monitor.py ghi lai: ax,ay,az,gx,gy,gz (g / rad/s)."""
    data = np.loadtxt(path, delimiter=',', ndmin=2)
    return data[:, 0:3], data[:, 3:6]


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


def load_ground_truth(path):
    """File .gt.txt: '<t_start> <t_end> <TEN_HOAT_DONG>' moi dong -> list tuple."""
    name_to_id = {v: k for k, v in cfg.ACTIVITY_LABELS.items()}
    out = []
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) < 3:
                continue
            out.append((float(parts[0]), float(parts[1]), name_to_id.get(parts[2], -1), parts[2]))
    return out
