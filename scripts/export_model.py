"""
Sinh code C tu models/ -> firmware/Core/{Inc,Src}/model_params.* + test/golden_vectors.h

Chay:
    python scripts/export_model.py
    python scripts/export_model.py --golden acc.txt gyro.txt   # doi chieu tu file do tay

Golden vectors (du lieu doi chieu C vs Python) CHI sinh tu tin hieu THAT:
  1. --golden <acc> <gyro>   neu co
  2. HAPT RawData            neu tim thay
  3. neu khong co gi         -> GIU NGUYEN file da co, khong ghi de
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from har import dataset, export_c, fall, model as har_model


def pick_golden(args):
    """-> (acc, gyro) hoac None neu khong co du lieu that -> giu nguyen file cu."""
    n = args.golden_samples
    if args.golden:
        acc, gyro = dataset.load_pair(*args.golden)
        print('Golden: tu file %s' % args.golden[0])
        return acc[:n], gyro[:n]
    try:
        raw, _ = dataset.load_raw(dataset.find_rawdata(args.data), verbose=False)
    except FileNotFoundError:
        print('\n  Khong tim thay HAPT RawData -> GIU NGUYEN golden_vectors.h.')
        print('  Golden vectors chi sinh tu tin hieu THAT. Muon sinh lai thi dat')
        print('  HAPT_RAWDATA hoac dung --golden <acc> <gyro>.\n')
        return None
    key = sorted(raw)[0]
    print('Golden: tu HAPT exp%02d user%02d, %d mau' % (key[0], key[1], n))
    return raw[key]['acc'][:n], raw[key]['gyro'][:n]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--models', default='models')
    ap.add_argument('--out', default='firmware')
    ap.add_argument('--data', default=None, help='duong dan HAPT RawData')
    ap.add_argument('--golden', nargs=2, metavar=('ACC', 'GYRO'), default=None)
    ap.add_argument('--golden-samples', type=int, default=384)
    args = ap.parse_args()

    scaler, clf, meta = har_model.load_bundle(args.models)
    if not meta:
        raise SystemExit('Thieu models/meta.json - chay scripts/train.py truoc')

    print('Model : %s | %d dac trung | %d tham so'
          % (meta['arch'], meta['n_features'], meta['params']))
    print('Nguong te nga: %s' % str(fall.FallParams()))

    golden = pick_golden(args)
    written = export_c.export(scaler, clf, meta, out_dir=args.out, golden=golden)
    print()
    for p in written:
        print('  %-44s %6.1f KB' % (p, os.path.getsize(p) / 1024))
    if golden is None:
        print('  %-44s %6s' % (os.path.join(args.out, 'test', 'golden_vectors.h'),
                               'giu nguyen'))
    print('\nKHONG SUA TAY cac file tren. Sua har/export_c.py roi chay lai.')


if __name__ == '__main__':
    main()
