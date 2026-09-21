"""
Huan luyen, danh gia va luu/nap bo model (scaler + classifier).

CHI DUNG ML CO DIEN - khong mang no-ron.

Model DUOC DEM XUONG MCU la LogisticRegression 120 -> 12 (tuyen tinh):
  - 12 x 120 + 12 = 1.452 tham so = 5.7 KB float32
  - suy luan 1 cua so = 1.440 phep MAC + softmax -> vai chuc microsecond tren Cortex-M4F
  - la mo hinh loi: nghiem duy nhat, khong phu thuoc seed

Cac lua chon da loai:
  - SVM RBF (repo DSP goc): 2636 support vector x 120 float = ~1.2 MB -> qua flash 512 KB
  - Random Forest 200 cay  : export ra if/else C cung vai tram KB
  - MLP / mang no-ron      : ngoai pham vi de tai (chi dung ML co dien); do thuc te cung
                             thua logreg o moi cau hinh dac trung
"""
import json
import os

import joblib
import numpy as np
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import (accuracy_score, classification_report, confusion_matrix,
                             f1_score, precision_score, recall_score)
from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC

from . import config as cfg


def build_models(which=('logreg', 'rf', 'svm')):
    """Cac model doi chieu cho bao cao. Zoo day du nam o scripts/train.py."""
    zoo = {
        'logreg': LogisticRegression(C=0.1, max_iter=2000, random_state=cfg.SEED),
        'rf': RandomForestClassifier(n_estimators=200, random_state=cfg.SEED),
        'svm': SVC(kernel='rbf', C=10, random_state=cfg.SEED),
    }
    return {k: zoo[k] for k in which}


def metrics(y_true, y_pred):
    return {
        'accuracy': accuracy_score(y_true, y_pred),
        'precision': precision_score(y_true, y_pred, average='weighted', zero_division=0),
        'recall': recall_score(y_true, y_pred, average='weighted', zero_division=0),
        'f1': f1_score(y_true, y_pred, average='weighted', zero_division=0),
    }


def print_metrics(name, m):
    print(f'=== {name} ===')
    for k in ('accuracy', 'precision', 'recall', 'f1'):
        print(f'  {k.capitalize():<10} {m[k] * 100:6.2f}%')


def report(y_true, y_pred):
    names = [cfg.ACTIVITY_LABELS[i] for i in sorted(set(y_true) | set(y_pred))]
    return classification_report(y_true, y_pred, target_names=names, zero_division=0)


def confusion(y_true, y_pred):
    return confusion_matrix(y_true, y_pred)


# ---------------------------------------------------------------------------
# Luu / nap
# ---------------------------------------------------------------------------
DEFAULT_DIR = 'models'


def save_bundle(out_dir, scaler, clf, meta):
    os.makedirs(out_dir, exist_ok=True)
    joblib.dump(scaler, os.path.join(out_dir, 'scaler.pkl'))
    joblib.dump(clf, os.path.join(out_dir, 'classifier.pkl'))
    with open(os.path.join(out_dir, 'meta.json'), 'w') as f:
        json.dump(meta, f, indent=2)
    print(f'Da luu model vao {out_dir}/ (scaler.pkl, classifier.pkl, meta.json)')


def load_bundle(out_dir=DEFAULT_DIR):
    scaler = joblib.load(os.path.join(out_dir, 'scaler.pkl'))
    clf = joblib.load(os.path.join(out_dir, 'classifier.pkl'))
    meta_path = os.path.join(out_dir, 'meta.json')
    meta = json.load(open(meta_path)) if os.path.isfile(meta_path) else {}
    return scaler, clf, meta


def linear_weights(clf):
    """Model tuyen tinh -> (W, b) float32 cho export_c.

    W shape (N_CLASSES, n_features), b shape (N_CLASSES,).
    Ben C: score[k] = b[k] + sum_j W[k][j] * x[j], lay argmax (softmax chi de lay do tin cay).
    """
    W = np.ascontiguousarray(clf.coef_, dtype=np.float32)
    b = np.ascontiguousarray(clf.intercept_, dtype=np.float32)
    return W, b
