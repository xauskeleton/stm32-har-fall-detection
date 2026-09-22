"""
Huan luyen, danh gia va luu/nap bo model (scaler + classifier).

Model dem xuong MCU: LogisticRegression 120 -> 12 (tuyen tinh).
  - 12 x 120 + 12 = 1452 tham so = 5.7 KB float32
  - suy luan 1 cua so = 1440 phep MAC + argmax
  - mo hinh loi: nghiem duy nhat, khong phu thuoc seed

Ly do chon (thang ca SVM RBF 1.2 MB lan Random Forest): docs/DECISIONS.md muc 3.
"""
import json
import os

import joblib
import numpy as np
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (accuracy_score, classification_report, confusion_matrix,
                             f1_score, precision_score, recall_score)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler

from . import config as cfg

DEFAULT_DIR = 'models'


def build(C=cfg.LOGREG_C):
    """Chuan hoa Z-score roi phan loai tuyen tinh."""
    return Pipeline([
        ('scaler', StandardScaler()),
        ('clf', LogisticRegression(C=C, max_iter=2000, random_state=cfg.SEED)),
    ])


def metrics(y_true, y_pred):
    return {
        'accuracy': accuracy_score(y_true, y_pred),
        'precision': precision_score(y_true, y_pred, average='weighted', zero_division=0),
        'recall': recall_score(y_true, y_pred, average='weighted', zero_division=0),
        'f1': f1_score(y_true, y_pred, average='weighted', zero_division=0),
        'macro_f1': f1_score(y_true, y_pred, average='macro', zero_division=0),
    }


def print_metrics(name, m):
    print('=== %s ===' % name)
    for k in ('accuracy', 'precision', 'recall', 'f1', 'macro_f1'):
        print('  %-10s %6.2f%%' % (k.replace('_', '-').capitalize(), m[k] * 100))


def report(y_true, y_pred):
    names = [cfg.ACTIVITY_LABELS[i] for i in sorted(set(y_true) | set(y_pred))]
    return classification_report(y_true, y_pred, target_names=names, zero_division=0)


def confusion(y_true, y_pred):
    return confusion_matrix(y_true, y_pred)


# ---------------------------------------------------------------------------
# Luu / nap
# ---------------------------------------------------------------------------

def save_bundle(out_dir, scaler, clf, meta):
    os.makedirs(out_dir, exist_ok=True)
    joblib.dump(scaler, os.path.join(out_dir, 'scaler.pkl'))
    joblib.dump(clf, os.path.join(out_dir, 'classifier.pkl'))
    with open(os.path.join(out_dir, 'meta.json'), 'w') as f:
        json.dump(meta, f, indent=2)
    print('Da luu model vao %s/ (scaler.pkl, classifier.pkl, meta.json)' % out_dir)


def load_bundle(out_dir=DEFAULT_DIR):
    scaler = joblib.load(os.path.join(out_dir, 'scaler.pkl'))
    clf = joblib.load(os.path.join(out_dir, 'classifier.pkl'))
    meta_path = os.path.join(out_dir, 'meta.json')
    meta = json.load(open(meta_path)) if os.path.isfile(meta_path) else {}
    return scaler, clf, meta


def linear_weights(clf):
    """-> (W, b) float32. W shape (N_CLASSES, N_FEATURES), b shape (N_CLASSES,).

    Ben C: score[k] = b[k] + sum_j W[k][j] * x_chuan[j], lay argmax.
    """
    return (np.ascontiguousarray(clf.coef_, dtype=np.float32),
            np.ascontiguousarray(clf.intercept_, dtype=np.float32))
