"""
THU NGHIEM TONG HOP - khong phai te nga that.

Muc dich: tra loi mot cau hoi duy nhat ma khong can mau te nga:
  "Khi chuyen sang tu the nam DIEN RA RAT NHANH, bo phan loai xuat ra nhan gi?"

Te nga = chuyen tu dung/ngoi sang nam trong ~0.5s.
Nam xuong chu dong trong HAPT = 2-3s.

Cach lam: lay cac doan STAND_TO_LIE / SIT_TO_LIE THAT trong HAPT roi NEN THOI GIAN
lai k lan (noi suy tuyen tinh), ghep vao giua doan truoc va doan sau, chay lai ca
duong ong. k=1 la nguyen ban (phai KHONG bao dong - do la nam chu dong).

GIOI HAN NANG (da do, khong phai phong doan):
  Nen thoi gian bang noi suy tuyen tinh LAM GIAM bien do dinh, khong tang.
  Do thuc te: a_max lon nhat chi 1.914 g < nguong impact_g = 2.0 g
  -> dieu kien soc KHONG BAO GIO thoa (0/118 ca o moi muc k >= 2).
  Te nga that co luc dap 3-10 g. Vi vay script nay CHI do duoc nhanh CHUOI NHAN,
  con nhanh SOC bi tat san ve mat cau truc.
  => KHONG duoc dung ket qua o day de ket luan ve ty le phat hien te nga.
     Cot "BAO DONG" thap o day KHONG co nghia la he thong se bo sot te nga that.

Chay:
    python scripts/pseudo_fall.py
"""
import argparse
import os
import sys

import joblib
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from har import config as cfg
from har import dataset, dsp, fall

CONTEXT = 300      # 6 s truoc va sau doan chuyen tu the


def compress(seg, k):
    """Nen doan (N,3) con N/k mau bang noi suy tuyen tinh."""
    n_new = max(int(round(len(seg) / k)), 2)
    t_old = np.arange(len(seg))
    t_new = np.linspace(0, len(seg) - 1, n_new)
    return np.column_stack([np.interp(t_new, t_old, seg[:, i]) for i in range(3)])


def build_case(acc, gyro, start, end, k):
    """Ghep: [truoc] + [doan chuyen tu the nen k lan] + [sau]."""
    lo = max(0, start - 1 - CONTEXT)
    hi = min(len(acc), end + CONTEXT)
    if end - start < 10 or hi - end < cfg.WINDOW or start - 1 - lo < cfg.WINDOW:
        return None
    a = np.vstack([acc[lo:start - 1], compress(acc[start - 1:end], k), acc[end:hi]])
    g = np.vstack([gyro[lo:start - 1], compress(gyro[start - 1:end], k), gyro[end:hi]])
    return a, g


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--data', default=None)
    ap.add_argument('--cache', default=None)
    ap.add_argument('--models', default='models')
    args = ap.parse_args()

    scaler = joblib.load(os.path.join(args.models, 'scaler.pkl'))
    clf = joblib.load(os.path.join(args.models, 'classifier.pkl'))
    raw, labels = dataset.load_raw(dataset.find_rawdata(args.data), cache=args.cache)

    cases = labels[(labels[:, 2] == 9) | (labels[:, 2] == 11)]
    print('Tim thay %d doan nam xuong chu dong (SIT_TO_LIE / STAND_TO_LIE)' % len(cases))
    print('Nen thoi gian k lan roi chay lai ca duong ong.\n')

    KS = [1, 2, 3, 4, 6]
    print('%5s %9s %7s %8s %8s %10s %9s' % ('k', 'thoi gian', 'so ca', 'BAO DONG',
                                            'ty le', '*_TO_LIE', 'a_max max'))
    print('-' * 66)
    for k in KS:
        n_ok = n_alarm = n_sawtrans = 0
        durs = []
        amax_top = 0.0
        for exp_id, user_id, act, start, end in cases:
            rec = raw.get((exp_id, user_id))
            if rec is None:
                continue
            built = build_case(rec['acc'], rec['gyro'], start, end, k)
            if built is None:
                continue
            a, g = built
            X = dsp.pipeline(a, g, mode='causal', use_butter=cfg.USE_BUTTER,
                             groups=cfg.GROUPS_FULL, use_fft=True)
            if X is None:
                continue
            pred = clf.predict(scaler.transform(X))
            _, amax = fall.shock_windows(a)
            if amax is not None:
                amax_top = max(amax_top, float(amax.max()))
            _, events = fall.run_sequence(pred, acc=a)
            n_ok += 1
            durs.append((end - start) / cfg.FS / k)
            if any(e[2] == 'FALL' for e in events):
                n_alarm += 1
            if any(int(v) in cfg.LABELS_LIE_DOWN_TRANSITION for v in pred):
                n_sawtrans += 1
        print('%5d %8.2fs %7d %8d %7.1f%% %9.1f%% %9.3f' % (
            k, np.mean(durs), n_ok, n_alarm, n_alarm / n_ok * 100,
            n_sawtrans / n_ok * 100, amax_top))
    print('-' * 66)
    print('k=1 la nguyen ban (nam chu dong) -> BAO DONG phai THAP. Dung nhu vay.')
    print('cot *_TO_LIE  = % ca van thay nhan chuyen tu the (nhan chan bao dong)')
    print('cot a_max max = dinh |a| LON NHAT trong tat ca cac ca')
    print('   -> so sanh voi impact_g = %.2f g. Thap hon tuc la nhanh SOC KHONG THE'
          % fall.FallParams().impact_g)
    print('      kich hoat, nen cot BAO DONG chi phan anh nhanh CHUOI NHAN.')
    print('\nKET LUAN DUNG DAN tu script nay: chuoi nhan MOT MINH khong phan biet duoc')
    print('nam nhanh voi nam cham. KHONG duoc suy ra ty le bo sot te nga that tu day.')


if __name__ == '__main__':
    main()
