"""
Huan luyen model: 120 dac trung -> StandardScaler -> LogisticRegression -> 12 lop.

Chay:
    python scripts/train.py
    python scripts/train.py --data /duong/dan/RawData --cache hapt_raw.npz
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from sklearn.linear_model import LogisticRegression
from sklearn.preprocessing import StandardScaler

from har import config as cfg
from har import dataset, model as har_model


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=None, help='duong dan HAPT RawData')
    ap.add_argument('--cache', default=None, help='file .npz cache tin hieu raw')
    ap.add_argument('--out', default='models')
    ap.add_argument('-C', type=float, default=cfg.LOGREG_C)
    args = ap.parse_args()

    raw, labels = dataset.load_raw(dataset.find_rawdata(args.data), cache=args.cache)
    X_tr, y_tr, X_te, y_te = dataset.build_features(raw, labels)

    scaler = StandardScaler().fit(X_tr)
    clf = LogisticRegression(C=args.C, max_iter=2000, random_state=cfg.SEED)
    clf.fit(scaler.transform(X_tr), y_tr)

    pred = clf.predict(scaler.transform(X_te))
    m = har_model.metrics(y_te, pred)
    arch = 'logreg %d-%d' % (cfg.N_FEATURES, cfg.N_CLASSES)
    har_model.print_metrics(arch, m)
    print()
    print(har_model.report(y_te, pred))

    n_par = clf.coef_.size + clf.intercept_.size
    har_model.save_bundle(args.out, scaler, clf, {
        'arch': arch, 'kind': 'logreg', 'C': args.C,
        'groups': list(cfg.GROUPS), 'n_features': cfg.N_FEATURES,
        'params': int(n_par), 'flash_kb_f32': round(n_par * 4 / 1024, 1),
        'fs': cfg.FS, 'window': cfg.WINDOW, 'step': cfg.STEP,
        'n_train': int(len(y_tr)), 'n_test': int(len(y_te)),
        'metrics': {k: round(v * 100, 2) for k, v in m.items()},
    })
    print('Tham so: %d (%.1f KB float32)' % (n_par, n_par * 4 / 1024))


if __name__ == '__main__':
    main()
