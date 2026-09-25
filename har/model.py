"""
Huan luyen, danh gia va luu/nap bo model (chon cot + scaler + classifier).

Model dem xuong MCU: LogisticRegression 160 -> 12 (tuyen tinh).
  - 12 x 160 + 12 = 1932 tham so = 7.5 KB float32
  - suy luan 1 cua so = 1920 phep MAC + argmax
  - mo hinh loi: nghiem duy nhat, khong phu thuoc seed

Bo model gom 3 phan, ap dung theo dung thu tu nay:
  1. selected.json  - 160 chi so cot giu lai trong 561 dac trung tinh ra
  2. scaler.pkl     - StandardScaler fit tren 160 cot do
  3. classifier.pkl - LogisticRegression

Ly do chon logreg (thang ca XGBoost 225 KB, SVM RBF 649 KB va Random Forest 428 KB):
docs/DECISIONS.md muc 2.
"""
import json
import os

import joblib
import numpy as np
from sklearn.metrics import (accuracy_score, classification_report,
                             f1_score, precision_score, recall_score)

from . import config as cfg

DEFAULT_DIR = 'models'

# 4 nhan chuyen tu the lien quan den nam - bo phat hien te nga song nho chung,
# nen day la chi so QUYET DINH khi so cac phuong an.
FALL_LABELS = [9, 10, 11, 12]


def metrics(y_true, y_pred):
    return {
        'accuracy': accuracy_score(y_true, y_pred),
        'precision': precision_score(y_true, y_pred, average='weighted', zero_division=0),
        'recall': recall_score(y_true, y_pred, average='weighted', zero_division=0),
        'f1': f1_score(y_true, y_pred, average='weighted', zero_division=0),
        'macro_f1': f1_score(y_true, y_pred, average='macro', zero_division=0),
        'fall_f1': f1_score(y_true, y_pred, average='macro', labels=FALL_LABELS,
                            zero_division=0),
    }


def print_metrics(name, m):
    print('=== %s ===' % name)
    for k in ('accuracy', 'precision', 'recall', 'f1', 'macro_f1', 'fall_f1'):
        print('  %-10s %6.2f%%' % (k.replace('_', '-').capitalize(), m[k] * 100))


def report(y_true, y_pred):
    names = [cfg.ACTIVITY_LABELS[i] for i in sorted(set(y_true) | set(y_pred))]
    return classification_report(y_true, y_pred, target_names=names, zero_division=0)


# ---------------------------------------------------------------------------
# Ap dung bo model
# ---------------------------------------------------------------------------

def transform(scaler, cols, X_all):
    """(n, 561) -> (n, 160) da chuan hoa. Dung truoc khi goi clf.predict()."""
    return scaler.transform(np.asarray(X_all)[:, cols])


def predict(scaler, clf, cols, X_all):
    return clf.predict(transform(scaler, cols, X_all))


# ---------------------------------------------------------------------------
# Luu / nap
# ---------------------------------------------------------------------------

def save_bundle(out_dir, scaler, clf, cols, meta):
    os.makedirs(out_dir, exist_ok=True)
    joblib.dump(scaler, os.path.join(out_dir, 'scaler.pkl'))
    joblib.dump(clf, os.path.join(out_dir, 'classifier.pkl'))
    with open(os.path.join(out_dir, 'selected.json'), 'w', newline='\n') as f:
        json.dump([int(c) for c in cols], f)
    with open(os.path.join(out_dir, 'meta.json'), 'w', newline='\n') as f:
        json.dump(meta, f, indent=2)
    print('Da luu model vao %s/ (selected.json, scaler.pkl, classifier.pkl, meta.json)'
          % out_dir)


def load_bundle(out_dir=DEFAULT_DIR):
    scaler = joblib.load(os.path.join(out_dir, 'scaler.pkl'))
    clf = joblib.load(os.path.join(out_dir, 'classifier.pkl'))
    with open(os.path.join(out_dir, 'selected.json')) as f:
        cols = json.load(f)
    meta_path = os.path.join(out_dir, 'meta.json')
    meta = json.load(open(meta_path)) if os.path.isfile(meta_path) else {}
    return scaler, clf, cols, meta
