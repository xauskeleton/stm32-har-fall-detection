"""
Danh gia bo phat hien te nga tren toan bo HAPT.

HAPT KHONG co te nga -> moi su kien FALL sinh ra deu la BAO NHAM.
Do duoc phia am (bao nham). Phia duong (bo sot te nga that) CHUA do duoc vi
chua co mau te nga nao - xem docs/TODO.md.

Chay 2 lan de tach nguyen nhan:
  oracle : nap nhan THAT tu labels.txt  -> loi con lai la do THIET KE state machine
  model  : nap nhan do bo phan loai ra  -> chenh lech so voi oracle la do BO PHAN LOAI

Chay:
    python scripts/eval_fall.py
    python scripts/eval_fall.py --rule labels    # bo qua nguong soc gia toc
"""
import argparse
import os
import sys
from collections import Counter

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from har import config as cfg
from har import dataset, dsp, fall, model as har_model


def window_labels(labels, exp_id, user_id, n_win):
    """Nhan that cho tung cua so: lay nhan phu diem GIUA cua so. 0 = khong co nhan."""
    out = np.zeros(n_win, dtype=int)
    seg = labels[(labels[:, 0] == exp_id) & (labels[:, 1] == user_id)]
    centers = np.arange(n_win) * cfg.STEP + cfg.WINDOW // 2
    for _, _, act, start, end in seg:
        out[(centers >= start - 1) & (centers < end)] = act
    return out


def run_experiment(rec, labels, key, scaler, clf, cols, params):
    X = dsp.pipeline(rec['acc'], rec['gyro'])
    if X is None:
        return None
    pred = har_model.predict(scaler, clf, cols, X)
    truth = window_labels(labels, key[0], key[1], len(X))

    out = {'n_win': len(X), 'truth': truth, 'pred': pred,
           'seconds': len(X) * fall.WINDOW_PERIOD}
    for src, seq in (('oracle', truth), ('model', pred)):
        _, events = fall.run_sequence(seq, acc=rec['acc'], params=params)
        out[src] = [e for e in events if e[2] == 'FALL']
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=None)
    ap.add_argument('--cache', default=None)
    ap.add_argument('--models', default='models')
    ap.add_argument('--rule', default=None, choices=['both', 'any', 'labels'])
    args = ap.parse_args()

    scaler, clf, cols, _ = har_model.load_bundle(args.models)
    params = fall.FallParams(shock_rule=args.rule) if args.rule else fall.FallParams()

    try:
        raw_dir = dataset.find_rawdata(args.data)
    except FileNotFoundError as e:
        raise SystemExit('\n  %s\n' % e)
    raw, labels = dataset.load_raw(raw_dir, cache=args.cache)
    results = []
    for key in sorted(raw):
        r = run_experiment(raw[key], labels, key, scaler, clf, cols, params)
        if r is not None:
            results.append(r)

    total_s = sum(r['seconds'] for r in results)
    print('\n' + '=' * 70)
    print('BAO NHAM TE NGA tren HAPT (du lieu KHONG co te nga)')
    print('nguong: %s' % str(params))
    print('=' * 70)
    print('%d experiment, %.1f phut lien tuc, %d cua so'
          % (len(results), total_s / 60, sum(r['n_win'] for r in results)))
    print()
    print('%-8s %10s %14s %16s' % ('nguon', 'bao nham', 'moi ... phut', 'experiment dinh'))
    print('-' * 70)
    for src in ('oracle', 'model'):
        n = sum(len(r[src]) for r in results)
        bad = sum(1 for r in results if r[src])
        print('%-8s %10d %14s %16s'
              % (src, n, ('%.1f' % (total_s / 60 / n)) if n else 'khong bao gio',
                 '%d/%d' % (bad, len(results))))
    print('-' * 70)

    for src in ('oracle', 'model'):
        ctx = Counter()
        for r in results:
            seq = r['truth'] if src == 'oracle' else r['pred']
            for i, _, _ in r[src]:
                ctx[' '.join(cfg.ACTIVITY_LABELS.get(int(v), '?')[:9]
                             for v in seq[max(0, i - 3):i + 1])] += 1
        if ctx:
            print('\nChuoi nhan ngay truoc moi lan bao nham (%s):' % src)
            for pat, n in ctx.most_common(6):
                print('  %3dx  %s' % (n, pat))

    print('\nSo lan nam xuong CHU DONG trong HAPT: %d'
          % int(((labels[:, 2] == 9) | (labels[:, 2] == 11)).sum()))


if __name__ == '__main__':
    main()
