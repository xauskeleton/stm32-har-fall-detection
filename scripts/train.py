"""
Train + so sanh cac cau hinh NHE de chon bo dem xuong STM32F446RET6.

Muc tieu: tim cau hinh re nhat ma accuracy khong tut qua ~2% so voi pipeline day du.

Truc so sanh:
  - Bo dac trung : co/khong nhom body_acc (= co/khong can bo loc 0.3Hz)
                   x co/khong dac trung FFT
  - Model        : CHI ML CO DIEN - tuyen tinh (logreg/linsvc/lda/ridge),
                   Naive Bayes, cay quyet dinh, rung ngau nhien, SVM RBF

Chay:
    python scripts/train.py --sweep            # bang so sanh, khong luu model
    python scripts/train.py                    # train cau hinh da chot, luu models/
"""
import argparse
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from sklearn.discriminant_analysis import LinearDiscriminantAnalysis
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression, RidgeClassifier
from sklearn.naive_bayes import GaussianNB
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC, LinearSVC
from sklearn.tree import DecisionTreeClassifier

from har import config as cfg
from har import dataset, model as har_model


# === Cac cau hinh dac trung ===
# needs_iir = firmware co phai chay bo loc IIR khong
FEATURE_CONFIGS = [
    # ten        groups             fft    butter  mode
    ('min48',    cfg.GROUPS_MIN,    False, False,  'causal'),
    ('min80',    cfg.GROUPS_MIN,    True,  False,  'causal'),
    ('body72',   cfg.GROUPS_FULL,   False, False,  'causal'),
    ('body120',  cfg.GROUPS_FULL,   True,  False,  'causal'),
    # tham chieu: pipeline day du cua repo DSP goc (butter 20Hz + filtfilt)
    ('dsp_full', cfg.GROUPS_FULL,   True,  True,   'zerophase'),
]


def build_zoo(seed=cfg.SEED):
    """Chi ML co dien - KHONG mang no-ron.

    4 model dau deu la TUYEN TINH: tren MCU chi la 1 phep nhan ma tran 12 x n_feat.
    Ba model cuoi nang hon, de doi chieu.
    """
    return [
        ('logreg', lambda: LogisticRegression(C=0.1, max_iter=2000, random_state=seed, multi_class='multinomial', solver='lbfgs')),
        ('linsvc', lambda: LinearSVC(C=0.1, max_iter=5000, random_state=seed)),
        ('lda', lambda: LinearDiscriminantAnalysis()),
        ('ridge', lambda: RidgeClassifier(random_state=seed)),
        ('gnb', lambda: GaussianNB()),
        ('tree', lambda: DecisionTreeClassifier(max_depth=12, random_state=seed)),
        ('rf50', lambda: RandomForestClassifier(n_estimators=50, max_depth=12,
                                                random_state=seed, n_jobs=-1)),
        ('svm_rbf', lambda: SVC(kernel='rbf', C=10, random_state=seed)),
    ]


def count_params(clf):
    """So tham so float32 -> uoc luong flash."""
    if hasattr(clf, 'estimators_'):                       # RandomForest
        return sum(t.tree_.node_count * 4 for t in clf.estimators_)
    if hasattr(clf, 'tree_'):                             # DecisionTree
        return clf.tree_.node_count * 4
    if hasattr(clf, 'support_vectors_'):                  # SVC RBF
        return (clf.support_vectors_.size + clf.dual_coef_.size
                + clf.intercept_.size)
    if hasattr(clf, 'theta_'):                            # GaussianNB: mean + var + prior
        return clf.theta_.size + clf.var_.size + clf.class_prior_.size
    return clf.coef_.size + clf.intercept_.size           # cac model tuyen tinh


def sweep(raw, labels, out_json=None):
    zoo = build_zoo()
    rows = []

    for name, groups, use_fft, use_butter, mode in FEATURE_CONFIGS:
        n_feat = cfg.n_features(groups, use_fft)
        t0 = time.time()
        X_tr, y_tr, u_tr, X_val, y_val, u_val, X_te, y_te, u_te = dataset.build_features(
            raw, labels, mode=mode, use_butter=use_butter,
            groups=groups, use_fft=use_fft)
        scaler = StandardScaler().fit(X_tr)
        Xs_tr, Xs_val = scaler.transform(X_tr), scaler.transform(X_val)
        print('[%s] %d features | %d train / %d val | trich xuat %.1fs'
              % (name, n_feat, X_tr.shape[0], X_val.shape[0], time.time() - t0))

        for mname, make in zoo:
            t1 = time.time()
            clf = make()
            clf.fit(Xs_tr, y_tr)
            m = har_model.metrics(y_val, clf.predict(Xs_val))
            row = {
                'config': name, 'groups': list(groups), 'fft': use_fft,
                'butter': use_butter, 'mode': mode, 'n_feat': n_feat,
                'model': mname, 'params': int(count_params(clf)),
                'needs_iir': bool('body_acc' in groups or use_butter),
                'fit_s': round(time.time() - t1, 1),
            }
            row.update({k: round(v * 100, 2) for k, v in m.items()})
            rows.append(row)
            print('    %-12s acc %6.2f%%  f1 %6.2f%%  %6d params  (%.1fs)'
                  % (mname, row['accuracy'], row['f1'], row['params'], row['fit_s']))

    print_table(rows)
    if out_json:
        with open(out_json, 'w') as f:
            json.dump(rows, f, indent=2)
        print('\nDa ghi ' + out_json)
    return rows


def print_table(rows):
    models = []
    for r in rows:
        if r['model'] not in models:
            models.append(r['model'])
    configs = []
    for r in rows:
        if r['config'] not in configs:
            configs.append(r['config'])

    w = 28 + 11 * len(models)
    print('\n' + '=' * w)
    print('SWEEP: bo dac trung x kich thuoc model  (accuracy % tap test, seed=42)')
    print('=' * w)
    head = '%-10s %5s %4s %4s ' % ('Cau hinh', '#Feat', 'IIR', 'FFT')
    for m in models:
        head += '%11s' % m
    print(head)
    print('-' * w)
    for c in configs:
        sub = [r for r in rows if r['config'] == c]
        r0 = sub[0]
        line = '%-10s %5d %4s %4s ' % (c, r0['n_feat'],
                                       'co' if r0['needs_iir'] else '-',
                                       'co' if r0['fft'] else '-')
        for m in models:
            hit = [r for r in sub if r['model'] == m]
            line += ('%10.2f%%' % hit[0]['accuracy']) if hit else ('%11s' % '-')
        print(line)
    print('=' * w)
    print('min*     = chi total_acc + gyro -> KHONG bo loc nao, firmware khong can biquad')
    print('body*    = them body_acc        -> can Butterworth 0.3Hz tach gravity')
    print('dsp_full = pipeline repo DSP goc (butter 20Hz + filtfilt, non-causal, chi de doi chieu)')


def train_final(raw, labels, groups, use_fft, mode, kind, C, out_dir):
    n_feat = cfg.n_features(groups, use_fft)
    X_tr, y_tr, u_tr, X_val, y_val, u_val, X_te, y_te, u_te = dataset.build_features(
        raw, labels, mode=mode, use_butter=False, groups=groups, use_fft=use_fft)
    scaler = StandardScaler().fit(X_tr)

    zoo = dict(build_zoo())
    if kind not in zoo:
        raise SystemExit('model khong hop le: %s (co: %s)' % (kind, ', '.join(zoo)))
    clf = zoo[kind]()
    if kind in ('logreg', 'linsvc'):
        clf.set_params(C=C)
    arch = '%s %d-%d' % (kind, n_feat, cfg.N_CLASSES)
    clf.fit(scaler.transform(X_tr), y_tr)

    pred = clf.predict(scaler.transform(X_val))
    m = har_model.metrics(y_val, pred)
    print("\n--- VALIDATION METRICS ---")
    har_model.print_metrics(arch, m)
    print(har_model.report(y_val, pred))

    n_par = int(count_params(clf))
    meta = {
        'groups': list(groups), 'use_fft': use_fft, 'use_butter': False,
        'mode': mode, 'n_features': n_feat, 'kind': kind, 'arch': arch,
        'linear': bool(hasattr(clf, 'coef_')),
        'C': C if kind in ('logreg', 'linsvc') else None,
        'params': n_par, 'flash_kb_f32': round(n_par * 4 / 1024, 1),
        'fs': cfg.FS, 'window': cfg.WINDOW, 'step': cfg.STEP,
        'metrics': {k: round(v * 100, 2) for k, v in m.items()},
    }
    har_model.save_bundle(out_dir, scaler, clf, meta)
    print('Tham so: %d (%.1f KB float32)' % (n_par, n_par * 4 / 1024))
    return meta


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=None, help='duong dan HAPT RawData')
    ap.add_argument('--cache', default=None, help='file .npz cache tin hieu raw')
    ap.add_argument('--sweep', action='store_true', help='chay bang so sanh')
    ap.add_argument('--out', default='models')
    ap.add_argument('--json', default='models/sweep.json')
    ap.add_argument('--groups', default='full', choices=['min', 'full'])
    ap.add_argument('--no-fft', action='store_true')
    ap.add_argument('--model', default='logreg',
                    choices=[n for n, _ in build_zoo()])
    ap.add_argument('-C', type=float, default=0.1, help='regularization cho logreg / linsvc')
    args = ap.parse_args()

    raw_dir = dataset.find_rawdata(args.data)
    raw, labels = dataset.load_raw(raw_dir, cache=args.cache)

    if args.sweep:
        os.makedirs(os.path.dirname(args.json) or '.', exist_ok=True)
        sweep(raw, labels, out_json=args.json)
        return

    groups = cfg.GROUPS_MIN if args.groups == 'min' else cfg.GROUPS_FULL
    train_final(raw, labels, groups, not args.no_fft, 'causal',
                args.model, args.C, args.out)


if __name__ == '__main__':
    main()
