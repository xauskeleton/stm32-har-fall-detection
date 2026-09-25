"""
Huan luyen model: 552 dac trung -> RFE giu 160 -> StandardScaler -> LogisticRegression.

Chia train/test theo NGUOI, dung split chinh thuc cua HAPT: 21 nguoi train, 9 nguoi test.
RFE chi chay tren tap train.

Chay:
    python scripts/train.py
    python scripts/train.py --data /duong/dan/RawData --cache hapt_raw.npz
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np
from sklearn.feature_selection import RFE
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler

from har import config as cfg
from har import dataset, dsp, model as har_model

ap = argparse.ArgumentParser()
ap.add_argument('--data', default=None, help='duong dan HAPT RawData')
ap.add_argument('--cache', default=None, help='file .npz cache tin hieu raw')
ap.add_argument('--out', default='models')
ap.add_argument('-C', type=float, default=cfg.LOGREG_C)
ap.add_argument('-k', type=int, default=cfg.N_FEATURES)
args = ap.parse_args()

# === DOC DU LIEU VA TINH DAC TRUNG ===
try:
    raw_dir = dataset.find_rawdata(args.data)
except FileNotFoundError as e:
    raise SystemExit('\n  %s\n' % e)

raw, labels = dataset.load_raw(raw_dir, cache=args.cache)
X_train, y_train, X_test, y_test = dataset.build_features(raw, labels)

# === CHON DAC TRUNG: RFE tren TAP TRAIN, khong dung tap test ===
print('Chon %d / %d dac trung bang RFE...' % (args.k, X_train.shape[1]), flush=True)
rfe = RFE(LogisticRegression(C=args.C, max_iter=1500, random_state=cfg.SEED),
          n_features_to_select=args.k, step=50)
rfe.fit(StandardScaler().fit_transform(X_train), y_train)
cols = sorted(int(i) for i in np.where(rfe.support_)[0])

# === HUAN LUYEN tren 160 cot da chon ===
scaler = StandardScaler().fit(X_train[:, cols])
clf = LogisticRegression(C=args.C, max_iter=4000, random_state=cfg.SEED)
clf.fit(scaler.transform(X_train[:, cols]), y_train)

# === DANH GIA tren tap test ===
pred = har_model.predict(scaler, clf, cols, X_test)
metrics = har_model.metrics(y_test, pred)
arch = 'logreg %d-%d' % (args.k, cfg.N_CLASSES)
har_model.print_metrics(arch, metrics)
print()
print(har_model.report(y_test, pred))

# === LUU MODEL ===
n_par = clf.coef_.size + clf.intercept_.size
har_model.save_bundle(args.out, scaler, clf, cols, {
    'arch': arch, 'kind': 'logreg', 'C': args.C,
    'signals': list(cfg.SIGNALS),
    'n_features_all': int(X_train.shape[1]), 'n_features': args.k,
    'selection': 'RFE tren tap train (21 nguoi)',
    'params': int(n_par), 'flash_kb_f32': round(n_par * 4 / 1024, 1),
    'fs': cfg.FS, 'window': cfg.WINDOW, 'step': cfg.STEP,
    'n_train': int(len(y_train)), 'n_test': int(len(y_test)),
    'metrics': {k: round(v * 100, 2) for k, v in metrics.items()},
})
print('Tham so: %d (%.1f KB float32)' % (n_par, n_par * 4 / 1024))

names = dsp.feature_names()
print('\n10 dac trung dau trong bo da chon: %s' % ', '.join(names[i] for i in cols[:10]))
