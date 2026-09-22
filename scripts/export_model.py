"""
Sinh code C tu models/ -> firmware/Core/{Inc,Src}/model_params.* + test/golden_vectors.h

Chay:
    python scripts/export_model.py
    python scripts/export_model.py --golden acc.txt gyro.txt   # doi chieu tu file do tay

Golden vectors (du lieu doi chieu C vs Python) UU TIEN tin hieu THAT:
  1. --golden <acc> <gyro>          neu co
  2. HAPT RawData                   neu tim thay
  3. GIU NGUYEN file da co          neu khong co du lieu that -> khong ghi de
  4. Tin hieu tong hop (sin)        chi khi chua co file nao, kem canh bao ro rang
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

from har import config as cfg
from har import dataset, export_c, fall, model as har_model


def synthetic(n_sample):
    """Tin hieu tong hop - CHI dung khi khong co du lieu that nao."""
    t = np.arange(n_sample) / cfg.FS
    acc = np.column_stack([0.3 * np.sin(2 * np.pi * 1.7 * t),
                           0.2 * np.cos(2 * np.pi * 2.3 * t),
                           1.0 + 0.25 * np.sin(2 * np.pi * 1.7 * t + 0.6)])
    gyro = np.column_stack([0.4 * np.sin(2 * np.pi * 1.1 * t),
                            0.3 * np.sin(2 * np.pi * 2.9 * t),
                            0.2 * np.cos(2 * np.pi * 0.7 * t)])
    return acc, gyro


def pick_golden(args, golden_path):
    """-> (golden, la_tong_hop) hoac (None, False) neu giu nguyen file cu."""
    n = args.golden_samples

    if args.golden:
        acc, gyro = dataset.load_pair(*args.golden)
        print('Golden: tu file %s' % args.golden[0])
        return (acc[:n], gyro[:n]), False

    try:
        raw, _ = dataset.load_raw(dataset.find_rawdata(args.data), verbose=False)
        key = sorted(raw)[0]
        print('Golden: tu HAPT exp%02d user%02d, %d mau' % (key[0], key[1], n))
        return (raw[key]['acc'][:n], raw[key]['gyro'][:n]), False
    except FileNotFoundError:
        pass

    if os.path.isfile(golden_path) and not args.force_golden:
        print('')
        print('  CANH BAO: khong tim thay HAPT RawData.')
        print('  -> GIU NGUYEN %s (sinh tu du lieu that).' % golden_path)
        print('     Khong ghi de bang tin hieu tong hop.')
        print('     Neu that su muon ghi de: them --force-golden')
        return None, False

    print('')
    print('  CANH BAO: khong co du lieu that -> golden vectors sinh tu TIN HIEU TONG HOP.')
    print('  Test doi chieu van dung duoc (tu nhat quan) nhung yeu hon: song sin khong')
    print('  phu het dai gia tri cua du lieu that. Nen chay lai khi co HAPT.')
    return synthetic(n), True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default='models')
    ap.add_argument('--out', default='firmware')
    ap.add_argument('--data', default=None, help='duong dan HAPT RawData')
    ap.add_argument('--golden', nargs=2, metavar=('ACC', 'GYRO'), default=None)
    ap.add_argument('--golden-samples', type=int, default=384)
    ap.add_argument('--force-golden', action='store_true',
                    help='cho phep ghi de golden_vectors.h bang tin hieu tong hop')
    args = ap.parse_args()

    scaler, clf, meta = har_model.load_bundle(args.models)
    if not meta:
        raise SystemExit('Thieu models/meta.json - chay scripts/train.py truoc')

    print('Model : %s' % meta['arch'])
    print('Nhom  : %s | %d dac trung | %d tham so'
          % (', '.join(meta['groups']), meta['n_features'], meta['params']))
    print('Nguong te nga: %s' % str(fall.FallParams()))

    golden_path = os.path.join(args.out, 'test', 'golden_vectors.h')
    golden, is_synth = pick_golden(args, golden_path)

    written = export_c.export(scaler, clf, meta, out_dir=args.out,
                              golden=golden, golden_synthetic=is_synth)
    print()
    for p in written:
        print('  %-44s %6.1f KB' % (p, os.path.getsize(p) / 1024))
    if golden is None:
        print('  %-44s %6s' % (golden_path, 'giu nguyen'))
    print('\nKHONG SUA TAY cac file tren. Sua har/export_c.py roi chay lai.')


if __name__ == '__main__':
    main()
