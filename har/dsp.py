"""
Khoi DSP: tach gravity -> cat cua so -> 120 dac trung.

MOT cau hinh duy nhat, da chot bang do dac (docs/DECISIONS.md):
  - KHONG loc lowpass 20 Hz (chi them ~0.5% accuracy, co truong hop con te hon)
  - Tach gravity: Butterworth bac 3, fc = 0.3 Hz, dang SOS, loc CAUSAL
  - 3 nhom x 4 kenh x 10 dac trung = 120

Loc causal (sosfilt) chu khong phai filtfilt: filtfilt chay forward-backward nen
can biet ca tin hieu tuong lai -> khong real-time duoc. Do thuc te causal khong
he kem hon (86.88% vs 86.50%).

Dang SOS de trung khop 1-1 voi cascade biquad ben firmware.
"""
import numpy as np
from scipy.fft import fft
from scipy.signal import butter, sosfilt

from . import config as cfg

# He so bo loc - xuat sang C boi har/export_c.py
SOS_GRAV = butter(cfg.FILTER_ORDER, cfg.FC_GRAV / (cfg.FS / 2), 'low', output='sos')


def median3(sig):
    """Loc median cua so 3 doc theo truc 0, giu nguyen do dai (2 bien copy nguyen).

    CHI dung cho kenh phat hien soc gia toc, KHONG dung cho dac trung phan loai.
    Xoa gai nhieu don le: gia toc ke cua HAPT co gai 1.49g trong 1 mau ngay khi
    nam yen, lam 12.9% cua so tu the tinh bi coi la te nga. Median 3 dua ve 0.0%.

    Viet don gian de port sang C khong sai lech: moi mau giua = gia tri o giua
    sau khi sap xep 3 mau lien tiep.
    """
    s = np.asarray(sig, dtype=np.float64)
    if len(s) < 3:
        return s.copy()
    out = s.copy()
    out[1:-1] = np.median(np.stack([s[:-2], s[1:-1], s[2:]]), axis=0)
    return out


def preprocess(acc, gyro):
    """Raw acc/gyro (N,3) -> 3 nhom tin hieu (total_acc, body_acc, gyro).

    Loc tren TOAN BO chuoi lien tuc TRUOC khi cat cua so, dung nhu firmware
    loc streaming voi state giu qua cac mau.
    """
    acc = np.asarray(acc, dtype=np.float64)
    gyro = np.asarray(gyro, dtype=np.float64)
    gravity = sosfilt(SOS_GRAV, acc, axis=0)
    return acc, acc - gravity, gyro


def make_windows(sig):
    """(N, C) -> (n_win, WINDOW, C), overlap 50%. None neu qua ngan."""
    sig = np.asarray(sig)
    n_win = (len(sig) - cfg.WINDOW) // cfg.STEP + 1
    if n_win < 1:
        return None
    idx = np.arange(n_win)[:, None] * cfg.STEP + np.arange(cfg.WINDOW)
    return sig[idx]


# ---------------------------------------------------------------------------
# 120 dac trung
# ---------------------------------------------------------------------------

def _channel_features(s):
    """s: (n_win, WINDOW) -> 10 vector (n_win,).

    Thu tu: mean, std, max, min, rms, mad, fft_max, fft_mean, fft_energy, mean_freq
    """
    half = s.shape[1] // 2
    fv = np.abs(fft(s, axis=1))[:, :half]          # pho 1 phia: 0 -> Nyquist
    freqs = np.arange(half) * cfg.FS / s.shape[1]
    return [
        np.mean(s, axis=1),                                    # bias / DC
        np.std(s, axis=1),                                     # do bien thien
        np.max(s, axis=1),                                     # peak duong
        np.min(s, axis=1),                                     # peak am
        np.sqrt(np.mean(s ** 2, axis=1)),                      # RMS
        np.median(np.abs(s - np.median(s, axis=1, keepdims=True)), axis=1),   # MAD
        np.max(fv, axis=1),                                    # bin manh nhat
        np.mean(fv, axis=1),                                   # bien do tb toan pho
        np.mean(fv ** 2, axis=1),                              # nang luong pho
        np.sum(freqs * fv, axis=1) / (np.sum(fv, axis=1) + 1e-10),   # trong tam pho
    ]


def group_features(win):
    """win: (n_win, WINDOW, 3) -> (n_win, 40)."""
    x, y, z = win[:, :, 0], win[:, :, 1], win[:, :, 2]
    mag = np.sqrt(x ** 2 + y ** 2 + z ** 2)     # bat bien voi huong dat cam bien
    feats = []
    for s in (x, y, z, mag):
        feats += _channel_features(s)
    return np.column_stack(feats)


def extract_features(win_acc, win_bacc, win_gyro):
    """3 nhom x 40 = 120 dac trung."""
    return np.hstack([group_features(win_acc),
                      group_features(win_bacc),
                      group_features(win_gyro)])


def feature_names():
    names = cfg.TIME_FEATURES + cfg.FREQ_FEATURES
    return ['%s_%s_%s' % (g, c, f)
            for g in cfg.GROUPS for c in cfg.CHANNELS for f in names]


def pipeline(acc, gyro):
    """Raw acc/gyro (N,3) -> ma tran dac trung (n_win, 120). None neu qua ngan."""
    a, b, g = preprocess(acc, gyro)
    wa = make_windows(a)
    if wa is None:
        return None
    return extract_features(wa, make_windows(b), make_windows(g))
