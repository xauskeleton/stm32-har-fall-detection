"""
Sinh code C tu models/ -> firmware/Core/{Inc,Src}/model_params.* + test/golden_vectors.h

Chay:
    python scripts/export_model.py
    python scripts/export_model.py --golden test/my_samples/acc_walking.txt \\
                                   test/my_samples/gyro_walking.txt
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

from har import config as cfg
from har import dataset, export_c, fall, model as har_model


def default_golden(n_sample=384):
    """Khong co file do tay -> lay tu HAPT. Khong co HAPT -> tin hieu tong hop."""
    try:
        raw, _ = dataset.load_raw(dataset.find_rawdata(), verbose=False)
        key = sorted(raw)[0]
        return raw[key]['acc'][:n_sample], raw[key]['gyro'][:n_sample]
    except FileNotFoundError:
        print('Khong tim thay HAPT -> dung tin hieu tong hop cho golden vectors')
        t = np.arange(n_sample) / cfg.FS
        acc = np.column_stack([0.3 * np.sin(2 * np.pi * 1.7 * t),
                               0.2 * np.cos(2 * np.pi * 2.3 * t),
                               1.0 + 0.25 * np.sin(2 * np.pi * 1.7 * t + 0.6)])
        gyro = np.column_stack([0.4 * np.sin(2 * np.pi * 1.1 * t),
                                0.3 * np.sin(2 * np.pi * 2.9 * t),
                                0.2 * np.cos(2 * np.pi * 0.7 * t)])
        return acc, gyro


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default='models')
    ap.add_argument('--out', default='firmware')
    ap.add_argument('--golden', nargs=2, metavar=('ACC', 'GYRO'), default=None)
    ap.add_argument('--golden-samples', type=int, default=384)
    args = ap.parse_args()

    scaler, clf, meta = har_model.load_bundle(args.models)
    if not meta:
        raise SystemExit('Thieu models/meta.json - chay scripts/train.py truoc')

    if args.golden:
        acc, gyro = dataset.load_pair(*args.golden)
        acc, gyro = acc[:args.golden_samples], gyro[:args.golden_samples]
    else:
        acc, gyro = default_golden(args.golden_samples)

    print('Model : %s' % meta['arch'])
    print('Nhom  : %s | %d dac trung | %d tham so'
          % (', '.join(meta['groups']), meta['n_features'], meta['params']))
    print('Nguong te nga: %s' % str(fall.FallParams()))

    written = export_c.export(scaler, clf, meta, out_dir=args.out,
                              golden=(acc, gyro))
    print()
    for p in written:
        print('  %-44s %6.1f KB' % (p, os.path.getsize(p) / 1024))
    print('\nKHONG SUA TAY cac file tren. Sua har/export_c.py roi chay lai.')


if __name__ == '__main__':
    main()
