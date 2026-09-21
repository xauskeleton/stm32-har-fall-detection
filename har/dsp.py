"""
Khoi DSP: (tuy chon loc) -> tach gravity -> cat cua so -> dac trung.

Toan bo dung dang SOS (second-order sections) de trung khop 1-1 voi cascade
biquad ben firmware neu can dung den.

CAU HINH NHE (cfg.GROUPS_MIN, use_butter=False): khong dung BAT KY bo loc nao
-> firmware khong can biquad, va khong con van de lech train/deploy.

Hai che do loc (chi co y nghia khi thuc su bat loc):
  - 'zerophase' (sosfiltfilt): giong repo DSP goc, KHONG tre pha, non-causal
    -> chi dung duoc khi xu ly offline ca file.
  - 'causal'    (sosfilt):     chay duoc real-time tren MCU, co tre pha.
    -> model dem nhung PHAI train o che do nay thi train/deploy moi khop.
"""
import numpy as np
from scipy.signal import butter, sosfilt, sosfiltfilt
from scipy.fft import fft

from . import config as cfg

# === He so bo loc (dung chung voi firmware) ===
SOS_NOISE = butter(cfg.FILTER_ORDER, cfg.FC_NOISE / (cfg.FS / 2), 'low', output='sos')
SOS_GRAV = butter(cfg.FILTER_ORDER, cfg.FC_GRAV / (cfg.FS / 2), 'low', output='sos')


def filter_signal(sig, sos, mode='causal'):
    """Loc tung truc cua mang (N, 3) hoac (N,).

    mode='causal'   -> sosfilt   (forward, co tre pha, khop firmware)
    mode='zerophase'-> sosfiltfilt (forward-backward, khong tre pha, offline)
    """
    axis = 0
    if mode == 'zerophase':
        return sosfiltfilt(sos, sig, axis=axis)
    if mode == 'causal':
        return sosfilt(sos, sig, axis=axis)
    raise ValueError(f"mode khong hop le: {mode!r} (chi nhan 'causal' | 'zerophase')")


def preprocess(acc, gyro, mode='causal', use_butter=cfg.USE_BUTTER,
               groups=cfg.GROUPS_FULL):
    """Raw acc/gyro (N,3) -> dict {ten_nhom: tin hieu lien tuc (N,3)}.

    Thu tu key trong dict theo dung thu tu `groups` -> quyet dinh thu tu feature.

    Loc tren TOAN BO chuoi lien tuc TRUOC khi cat cua so:
      - causal   : khop dung voi firmware (loc streaming, state giu qua cac mau)
      - zerophase: tranh edge effect cua filtfilt neu loc tren tung cua so 128 mau

    use_butter=False -> bo qua lowpass 20Hz. Neu 'body_acc' cung khong nam trong
    groups thi KHONG con phep loc nao duoc goi.
    """
    acc = np.asarray(acc, dtype=np.float64)
    gyro = np.asarray(gyro, dtype=np.float64)

    if use_butter:
        acc = filter_signal(acc, SOS_NOISE, mode)
        gyro = filter_signal(gyro, SOS_NOISE, mode)

    out = {}
    for g in groups:
        if g == 'total_acc':
            out[g] = acc
        elif g == 'gyro':
            out[g] = gyro
        elif g == 'body_acc':
            out[g] = acc - filter_signal(acc, SOS_GRAV, mode)
        else:
            raise ValueError(f'nhom khong hop le: {g!r}')
    return out


def median3(sig):
    """Loc median cua so 3 doc theo truc 0, giu nguyen do dai (2 bien copy nguyen).

    Viet don gian de port sang C khong sai lech: moi mau giua = median cua 3 mau
    lien tiep = gia tri o giua sau khi sap xep 3 phan tu.
    """
    s = np.asarray(sig, dtype=np.float64)
    if len(s) < 3:
        return s.copy()
    out = s.copy()
    out[1:-1] = np.median(np.stack([s[:-2], s[1:-1], s[2:]]), axis=0)
    return out


def make_windows(sig, window=cfg.WINDOW, step=cfg.STEP):
    """(N, C) -> (n_win, window, C), overlap 50%. Tra ve None neu qua ngan."""
    sig = np.asarray(sig)
    n_win = (len(sig) - window) // step + 1
    if n_win < 1:
        return None
    idx = np.arange(n_win)[:, None] * step + np.arange(window)
    return sig[idx]


# ---------------------------------------------------------------------------
# Trich xuat dac trung
# ---------------------------------------------------------------------------
# Thu tu feature (PHAI giu nguyen, firmware doc theo dung thu tu nay):
#   for group in (total_acc, body_acc, gyro):
#       for ch in (x, y, z, mag):
#           mean, std, max, min, rms, mad, fft_max, fft_mean, fft_energy, mean_freq

def _channel_features(s, use_fft=True):
    """s: (n_win, WINDOW) -> list cac vector (n_win,)."""
    out = [
        np.mean(s, axis=1),                                    # bias / DC
        np.std(s, axis=1),                                     # do bien thien
        np.max(s, axis=1),                                     # peak duong
        np.min(s, axis=1),                                     # peak am
        np.sqrt(np.mean(s ** 2, axis=1)),                      # RMS - nang luong tb
        np.median(np.abs(s - np.median(s, axis=1, keepdims=True)), axis=1),   # MAD
    ]
    if not use_fft:
        return out

    # Pho 1 phia: tin hieu thuc -> |FFT| doi xung, chi lay 0 .. Nyquist
    half = s.shape[1] // 2
    fv = np.abs(fft(s, axis=1))[:, :half]
    freqs = np.arange(half) * cfg.FS / s.shape[1]
    out += [
        np.max(fv, axis=1),                                    # bin manh nhat
        np.mean(fv, axis=1),                                   # bien do tb toan pho
        np.mean(fv ** 2, axis=1),                              # nang luong pho
        np.sum(freqs * fv, axis=1) / (np.sum(fv, axis=1) + 1e-10),   # trong tam pho
    ]
    return out


def group_features(win, use_fft=True):
    """win: (n_win, WINDOW, 3) -> (n_win, 40) [hoac 24 neu use_fft=False]."""
    x, y, z = win[:, :, 0], win[:, :, 1], win[:, :, 2]
    mag = np.sqrt(x ** 2 + y ** 2 + z ** 2)     # bat bien voi huong dat cam bien
    feats = []
    for s in (x, y, z, mag):
        feats += _channel_features(s, use_fft)
    return np.column_stack(feats)


def extract_features(wins, groups=cfg.GROUPS_FULL, use_fft=True):
    """wins: dict {ten_nhom: (n_win, WINDOW, 3)} -> (n_win, n_features).

    Ghep theo dung thu tu `groups`: for group: for channel: for feature.
    """
    return np.hstack([group_features(wins[g], use_fft) for g in groups])


def feature_names(groups=cfg.GROUPS_FULL, use_fft=True):
    names = cfg.TIME_FEATURES + (cfg.FREQ_FEATURES if use_fft else ())
    return [f'{g}_{c}_{f}' for g in groups for c in cfg.CHANNELS for f in names]


def pipeline(acc, gyro, mode='causal', use_butter=cfg.USE_BUTTER,
             groups=cfg.GROUPS_FULL, use_fft=True):
    """Raw acc/gyro (N,3) -> ma tran dac trung (n_win, n_features). None neu qua ngan."""
    sigs = preprocess(acc, gyro, mode=mode, use_butter=use_butter, groups=groups)
    wins = {g: make_windows(s) for g, s in sigs.items()}
    if wins[groups[0]] is None:
        return None
    return extract_features(wins, groups=groups, use_fft=use_fft)
