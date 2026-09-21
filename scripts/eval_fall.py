"""
Danh gia bo phat hien te nga tren toan bo HAPT.

HAPT KHONG co te nga -> moi su kien FALL sinh ra o day deu la BAO NHAM.
Do duoc phia am (false alarm). Phia duong (bo sot te nga that) chua do duoc
vi chua co mau te nga - xem docs/TODO.md.

Chay 2 lan de tach nguyen nhan:
  - oracle : nap nhan THAT tu labels.txt  -> loi con lai la do THIET KE state machine
  - model  : nap nhan do bo phan loai ra  -> chenh lech so voi oracle la do BO PHAN LOAI

Chay:
    python scripts/eval_fall.py
    python scripts/eval_fall.py --rule labels     # bo qua nguong soc gia toc
"""
import argparse
import os
import sys
from collections import Counter

import joblib
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from har import config as cfg
from har import dataset, dsp, fall


def window_labels(labels, exp_id, user_id, n_win, n_sample):
    """Nhan that cho tung cua so: lay nhan phu diem GIUA cua so. 0 = khong co nhan."""
    out = np.zeros(n_win, dtype=int)
    seg = labels[(labels[:, 0] == exp_id) & (labels[:, 1] == user_id)]
    centers = np.arange(n_win) * cfg.STEP + cfg.WINDOW // 2
    for _, _, act, start, end in seg:
        out[(centers >= start - 1) & (centers < end)] = act
    return out


def run_experiment(rec, labels, key, scaler, clf, params):
    """Tra ve dict ket qua cho 1 experiment."""
    exp_id, user_id = key
    sigs = dsp.preprocess(rec['acc'], rec['gyro'], mode='causal',
                          use_butter=cfg.USE_BUTTER, groups=cfg.GROUPS_FULL)
    wins = {g: dsp.make_windows(s) for g, s in sigs.items()}
    if wins['total_acc'] is None:
        return None
    n_win = len(wins['total_acc'])

    X = dsp.extract_features(wins, groups=cfg.GROUPS_FULL, use_fft=True)
    pred = clf.predict(scaler.transform(X))
    truth = window_labels(labels, exp_id, user_id, n_win, len(rec['acc']))

    out = {'key': key, 'n_win': n_win, 'truth': truth, 'pred': pred,
           'seconds': n_win * fall.WINDOW_PERIOD}
    for src in ('oracle', 'model'):
        seq = truth if src == 'oracle' else pred
        _, events = fall.run_sequence(seq, acc=rec['acc'], params=params)
        out[src] = [e for e in events if e[2] == 'FALL']
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=None)
    ap.add_argument('--cache', default=None)
    ap.add_argument('--models', default='models')
    ap.add_argument('--rule', default=None, choices=['both', 'any', 'labels'],
                    help='ghi de shock_rule')
    ap.add_argument('--laying-confirm', type=int, default=None)
    args = ap.parse_args()

    scaler = joblib.load(os.path.join(args.models, 'scaler.pkl'))
    clf = joblib.load(os.path.join(args.models, 'classifier.pkl'))

    kw = {}
    if args.rule:
        kw['shock_rule'] = args.rule
    if args.laying_confirm is not None:
        kw['laying_confirm'] = args.laying_confirm
    params = fall.FallParams(**kw)

    raw_dir = dataset.find_rawdata(args.data)
    raw, labels = dataset.load_raw(raw_dir, cache=args.cache)

    results = []
    for key in sorted(raw):
        r = run_experiment(raw[key], labels, key, scaler, clf, params)
        if r:
            results.append(r)

    total_s = sum(r['seconds'] for r in results)
    print('\n' + '=' * 72)
    print('BAO NHAM TE NGA tren HAPT (du lieu KHONG co te nga)')
    print('nguong: %s' % str(params))
    print('=' * 72)
    print('%d experiment, %.1f phut du lieu lien tuc, %d cua so'
          % (len(results), total_s / 60, sum(r['n_win'] for r in results)))
    print()
    print('%-8s %10s %14s %16s' % ('nguon', 'bao nham', 'moi ... phut', 'experiment dinh'))
    print('-' * 72)
    for src in ('oracle', 'model'):
        n = sum(len(r[src]) for r in results)
        bad = sum(1 for r in results if r[src])
        rate = (total_s / 60 / n) if n else float('inf')
        print('%-8s %10d %14s %16s' % (
            src, n, ('%.1f' % rate) if n else 'khong bao gio',
            '%d/%d' % (bad, len(results))))
    print('-' * 72)

    # Nhan nao dang truoc luc bao nham -> chi ra nguyen nhan
    for src in ('oracle', 'model'):
        ctx = Counter()
        for r in results:
            seq = r['truth'] if src == 'oracle' else r['pred']
            for i, _, _ in r[src]:
                lo = max(0, i - 3)
                ctx[' '.join(cfg.ACTIVITY_LABELS.get(int(v), '?')[:9]
                             for v in seq[lo:i + 1])] += 1
        if ctx:
            print('\nChuoi nhan ngay truoc moi lan bao nham (%s):' % src)
            for pat, n in ctx.most_common(6):
                print('  %3dx  %s' % (n, pat))

    # Co bao nham dung luc nguoi ta nam xuong chu dong khong?
    print('\nSo lan nam xuong CHU DONG trong HAPT: %d'
          % int(((labels[:, 2] == 9) | (labels[:, 2] == 11)).sum()))


if __name__ == '__main__':
    main()
