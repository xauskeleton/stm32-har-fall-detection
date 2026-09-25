"""
Khoi DSP: tach gravity -> dao ham -> cat cua so -> 552 dac trung.

MOT duong tien xu ly duy nhat, da chot bang do dac (docs/DECISIONS.md muc 4):

    median 3  ->  lowpass 20 Hz  ->  tach gravity 0.3 Hz  ->  cat cua so  ->  552 dac trung

Dung pipeline chinh thuc cua UCI HAR. Median + lowpass 20 Hz co ich vi bo dac trung co
JERK (dao ham theo thoi gian), ma dao ham khuech dai nhieu tan so cao ti le voi tan so:
tren bo 552 thi cau hinh nay thang 6/7 fold o F1 te nga (75.88 vs 73.08). Tren bo 120
dac trung cu (khong co jerk) thi no lam te hon - do la ly do ban truoc bo di.

Danh doi: bao dong sai dau-cuoi tang tu 0 len 2 tren 372 phut HAPT. Xem DECISIONS.md muc 4.

Kenh phat hien soc gia toc dung median 3 nhung KHONG dung lowpass: no can giu nguyen
dinh va dap, lowpass se lam cut dinh.

Loc causal (sosfilt) chu khong phai filtfilt: filtfilt chay forward-backward nen can
biet ca tin hieu tuong lai -> khong real-time duoc. Dang SOS de trung khop 1-1 voi
cascade biquad ben firmware.

Cong thuc tung phep lay dung theo ban dac trung 561 chinh thuc cua HAPT, da doi chieu
|r| = 1.000000 tung phep (docs/DECISIONS.md muc 3). Khac ban chinh thuc o 45 cot, deu
co ly do do duoc:
  - 9 cot 'sma' cua kenh do lon: trung khop 'mean' tung bit vi do lon luon >= 0
  - 36 cot khong tinh lai duoc trong float32: entropy/arCoeff/correlation cua nhom
    gravity, va max_ind cua moi nhom (xem DECISIONS.md muc 7)
"""
import numpy as np
from scipy.fft import rfft
from scipy.signal import butter, sosfilt, sosfilt_zi

from . import config as cfg

# He so 2 bo loc - xuat sang C boi har/export_c.py
SOS_NOISE = butter(cfg.FILTER_ORDER, cfg.FC_NOISE / (cfg.FS / 2), 'low', output='sos')
SOS_GRAV = butter(cfg.FILTER_ORDER, cfg.FC_GRAV / (cfg.FS / 2), 'low', output='sos')

# Trang thai xac lap cho dau vao = 1.0. Nhan voi mau dau tien de khoi tao bo loc, nho do
# khong co transient. Khong lam thi gravity sai tan 0.92 g o mau dau, phai ~4.4 s moi on
# -> 4 cua so dau tien sai nhan sau moi lan cap dien. Firmware PHAI lam y het.
SOS_NOISE_ZI = sosfilt_zi(SOS_NOISE)
SOS_GRAV_ZI = sosfilt_zi(SOS_GRAV)


def lowpass(x, sos, zi1):
    """Loc causal tung truc, state khoi tao ve xac lap theo mau dau tien."""
    out = np.empty_like(x)
    for k in range(x.shape[1]):
        out[:, k], _ = sosfilt(sos, x[:, k], zi=zi1 * x[0, k])
    return out

# Tan so cua 64 bin pho (bin 1..64, bo bin DC): 0.39 Hz -> 25 Hz
FREQS = np.arange(1, cfg.N_SPECTRUM + 1) * cfg.FS / cfg.WINDOW


# ---------------------------------------------------------------------------
# Tien xu ly: tin hieu lien tuc -> cua so
# ---------------------------------------------------------------------------

def median3(sig):
    """Loc median cua so 3: moi mau doi thanh gia tri GIUA cua 3 mau lien tiep.

    Vi du [1.0, 9.0, 1.0]: mau giua doi tu 9.0 thanh 1.0. Hai mau hai dau giu nguyen.
    Tac dung: xoa gai nhieu chi keo dai 1 mau.
    Dung cho CA duong dac trung VA kenh phat hien soc.
    """
    s = np.asarray(sig, dtype=np.float64)
    if len(s) < 3:
        return s.copy()
    out = s.copy()
    out[1:-1] = np.median(np.stack([s[:-2], s[1:-1], s[2:]]), axis=0)
    return out


def deriv(sig):
    """Dao ham theo thoi gian, sai phan LUI: (x[n] - x[n-1]) * fs, mau dau = 0.

    Dung sai phan lui chu khong phai tien vi sai phan tien can mau x[n+1], tuc
    non-causal - khong chay streaming duoc. Xem docs/DECISIONS.md muc 5.
    Ban C: har_pipeline.c tinh dung cong thuc nay, mau dau tien cung bang 0.
    """
    x = np.asarray(sig, dtype=np.float64)
    d = np.zeros_like(x)
    d[1:] = np.diff(x, axis=0) * cfg.FS
    return d


def preprocess(acc, gyro):
    """Raw acc/gyro (N,3) -> dict 5 nhom tin hieu lien tuc, moi nhom (N,3).

    median 3 -> lowpass 20 Hz -> tach gravity 0.3 Hz.
    Loc tren TOAN BO chuoi lien tuc TRUOC khi cat cua so, dung nhu firmware loc
    streaming voi state giu qua cac mau.
    """
    acc = lowpass(median3(acc), SOS_NOISE, SOS_NOISE_ZI)
    gyro = lowpass(median3(gyro), SOS_NOISE, SOS_NOISE_ZI)
    gravity = lowpass(acc, SOS_GRAV, SOS_GRAV_ZI)
    body = acc - gravity
    return {'body_acc': body, 'gravity': gravity, 'body_acc_jerk': deriv(body),
            'gyro': gyro, 'gyro_jerk': deriv(gyro)}


def make_windows(sig):
    """Cat thanh cac cua so 128 mau, truot 64 mau (overlap 50%).

    (N, C) -> (so_cua_so, 128, C). None neu tin hieu ngan hon 1 cua so.
    Cua so 0 la mau 0..127, cua so 1 la mau 64..191, cua so 2 la mau 128..255...
    """
    sig = np.asarray(sig)
    n_win = (len(sig) - cfg.WINDOW) // cfg.STEP + 1
    if n_win < 1:
        return None
    return np.stack([sig[i * cfg.STEP:i * cfg.STEP + cfg.WINDOW] for i in range(n_win)])


def spectrum(win):
    """(n_win, 128, C) -> (n_win, 64, C): do lon FFT bin 1..64, BO bin DC, chia 64.

    Bo bin DC vi no chi lap lai dac trung 'mean' o mien thoi gian.
    Chia 64 de ra bien do 1 phia - BAT BUOC, vi entropy() dung o co dinh nen khong
    bat bien theo ti le; khong chia thi moi gia tri > 1 va bi ep het vao mot o.
    """
    return np.abs(rfft(win, axis=1))[:, 1:cfg.N_SPECTRUM + 1, ...] / cfg.SPEC_SCALE


# ---------------------------------------------------------------------------
# Ba phep phai viet tay (numpy khong co san)
# ---------------------------------------------------------------------------

def entropy(s):
    """Entropy histogram 256 o CO DINH tren [0,1]: o = round(x*255), ep ve bien.

    Dung dung ham entropy() cua MATLAB Image Processing Toolbox ma tac gia HAPT da
    dung. O co dinh nen phep nay KHONG bat bien theo ti le - do la co y.
    """
    nb = cfg.ENTROPY_BINS
    b = np.clip(np.rint(s * (nb - 1)), 0, nb - 1).astype(np.int64)
    out = np.empty(len(b))
    for i in range(len(b)):
        c = np.bincount(b[i], minlength=nb)
        p = c[c > 0] / b.shape[1]
        out[i] = -(p * np.log2(p)).sum()
    return out


def burg(s):
    """He so mo hinh AR bac 4 bang thuat toan Burg -> (n_win, 4).

    Mo hinh AR mo ta tin hieu bang chinh no o cac mau truoc:
        x[n] = -(a1*x[n-1] + a2*x[n-2] + a3*x[n-3] + a4*x[n-4]) + nhieu
    Burg tim a1..a4 sao cho tong sai so tien va sai so lui nho nhat.
    """
    p, n = cfg.AR_ORDER, s.shape[1]
    out = np.empty((len(s), p))
    for i in range(len(s)):
        x = np.asarray(s[i], dtype=np.float64)
        fwd, bwd = x.copy(), x.copy()          # sai so tien, sai so lui
        a = np.zeros(p + 1)
        a[0] = 1.0
        den = 2.0 * (x * x).sum() - x[0] ** 2 - x[-1] ** 2
        for k in range(p):
            mu = 2.0 * np.dot(fwd[k + 1:n], bwd[0:n - k - 1]) / den if den else 0.0
            a_new = a.copy()
            for m in range(1, k + 2):
                a_new[m] = a[m] - mu * a[k + 1 - m]
            a = a_new
            f_next = fwd[k + 1:n] - mu * bwd[0:n - k - 1]
            bwd[0:n - k - 1] = bwd[0:n - k - 1] - mu * fwd[k + 1:n]
            fwd[k + 1:n] = f_next
            den = (1.0 - mu * mu) * den - fwd[k + 1] ** 2 - bwd[n - k - 2] ** 2
        out[i] = a[1:]
    return out


def iqr(s):
    """Khoang tu phan vi. Quy uoc MATLAB (i-0.5)/n = method='midpoint' khi n = 128."""
    q = np.percentile(s, [25, 75], axis=1, method='midpoint')
    return q[1] - q[0]


def mad(s):
    """Do lech tuyet doi TRUNG BINH (khong phai median absolute deviation)."""
    return np.abs(s - s.mean(axis=1, keepdims=True)).mean(axis=1)


def corr(a, b):
    """Tuong quan Pearson giua 2 kenh trong cung cua so."""
    a = a - a.mean(axis=1, keepdims=True)
    b = b - b.mean(axis=1, keepdims=True)
    return (a * b).sum(axis=1) / np.sqrt((a * a).sum(axis=1) * (b * b).sum(axis=1) + 1e-30)


def angle_to(v, g):
    """Goc giua 2 vector, tru pi/2 de ra khoang [-pi/2, pi/2]."""
    d = (v * g).sum(axis=1) / ((np.linalg.norm(v, axis=1) + 1e-30)
                               * (np.linalg.norm(g, axis=1) + 1e-30))
    return np.arccos(np.clip(d, -1.0, 1.0)) - np.pi / 2


# ---------------------------------------------------------------------------
# 552 dac trung
# ---------------------------------------------------------------------------

def extract_features(win):
    """win: dict ten_nhom -> (n_win, 128, 3). Tra ve (ten[], ma tran (n_win, 552)).

    Ten sinh ra CUNG LUC voi gia tri nen khong bao gio lech nhau.

    Da BO cac dac trung khong tinh lai duoc trong float32 (xem DECISIONS.md muc 7):
      - nhom gravity: bo entropy, arCoeff, correlation (tin hieu gan hang so -> mat
        dieu kien so; Burg cho sai so 5.8e18 giua C va Python)
      - moi nhom: bo max_ind (argmax roi rac, lech 1e-7 la nhay bin, sai toi 9 bin)

    Thu tu: 4 nhom 3 truc x 40 + gravity 22 -> 4 kenh do lon x 12 + gravity mag 7
            -> 3 pho 3 truc x 76 -> 4 pho do lon x 11 -> 7 goc = 516.
    """
    names, cols = [], []

    def add(name, value):
        names.append(name)
        cols.append(value)

    # === 5 nhom tin hieu 3 truc ===
    for g in cfg.SIGNALS:
        w = win[g]
        stable = g not in cfg.SIGNALS_NEAR_CONST
        for ax, c in enumerate(cfg.CHANNELS):
            s = w[:, :, ax]
            add('t_%s_%s_mean' % (g, c), s.mean(axis=1))              # bias / DC
            add('t_%s_%s_std' % (g, c), s.std(axis=1))                # do bien thien
            add('t_%s_%s_mad' % (g, c), mad(s))                       # do phan tan
            add('t_%s_%s_max' % (g, c), s.max(axis=1))                # peak duong
            add('t_%s_%s_min' % (g, c), s.min(axis=1))                # peak am
            add('t_%s_%s_energy' % (g, c), (s * s).mean(axis=1))      # nang luong tb
            add('t_%s_%s_iqr' % (g, c), iqr(s))                       # phan tan robust
            if stable:
                add('t_%s_%s_entropy' % (g, c), entropy(s))           # do "loan" cua bien do
                ar = burg(s)
                for j in range(cfg.AR_ORDER):
                    add('t_%s_%s_ar%d' % (g, c, j + 1), ar[:, j])     # dang song
        add('t_%s_sma' % g, np.abs(w).sum(axis=(1, 2)) / cfg.WINDOW)  # do lon trung binh
        if stable:
            for p, q, nm in ((0, 1, 'xy'), (0, 2, 'xz'), (1, 2, 'yz')):
                add('t_%s_corr_%s' % (g, nm), corr(w[:, :, p], w[:, :, q]))  # phoi hop 2 truc

    # === 5 kenh do lon mien thoi gian (khong sma, khong correlation) ===
    for g in cfg.SIGNALS:
        s = np.sqrt((win[g] ** 2).sum(axis=2))    # bat bien voi huong dat cam bien
        add('t_%s_mag_mean' % g, s.mean(axis=1))
        add('t_%s_mag_std' % g, s.std(axis=1))
        add('t_%s_mag_mad' % g, mad(s))
        add('t_%s_mag_max' % g, s.max(axis=1))
        add('t_%s_mag_min' % g, s.min(axis=1))
        add('t_%s_mag_energy' % g, (s * s).mean(axis=1))
        add('t_%s_mag_iqr' % g, iqr(s))
        if g not in cfg.SIGNALS_NEAR_CONST:
            add('t_%s_mag_entropy' % g, entropy(s))
            ar = burg(s)
            for j in range(cfg.AR_ORDER):
                add('t_%s_mag_ar%d' % (g, j + 1), ar[:, j])

    # === 3 pho 3 truc: 12 phep x 3 truc + sma + 14 dai x 3 truc = 79 ===
    for g in cfg.SIGNALS_SPEC:
        sp = spectrum(win[g])
        for ax, c in enumerate(cfg.CHANNELS):
            s = sp[:, :, ax]
            m = s.mean(axis=1, keepdims=True)
            sd = s.std(axis=1) + 1e-30
            add('f_%s_%s_mean' % (g, c), s.mean(axis=1))
            add('f_%s_%s_std' % (g, c), s.std(axis=1))
            add('f_%s_%s_mad' % (g, c), mad(s))
            add('f_%s_%s_max' % (g, c), s.max(axis=1))                # bin manh nhat
            add('f_%s_%s_min' % (g, c), s.min(axis=1))
            add('f_%s_%s_energy' % (g, c), (s * s).mean(axis=1))      # nang luong pho
            add('f_%s_%s_iqr' % (g, c), iqr(s))
            add('f_%s_%s_entropy' % (g, c), entropy(s))               # pho bet hay co dinh
            add('f_%s_%s_mean_freq' % (g, c),
                (s * FREQS).sum(axis=1) / (s.sum(axis=1) + 1e-30))    # trong tam pho
            add('f_%s_%s_skew' % (g, c), ((s - m) ** 3).mean(axis=1) / sd ** 3)
            add('f_%s_%s_kurt' % (g, c), ((s - m) ** 4).mean(axis=1) / sd ** 4)
        add('f_%s_sma' % g, np.abs(sp).sum(axis=(1, 2)) / cfg.N_SPECTRUM)
        power = sp * sp
        for ax, c in enumerate(cfg.CHANNELS):
            for lo, hi in cfg.BANDS:
                add('f_%s_%s_band%d_%d' % (g, c, lo, hi),
                    power[:, lo - 1:hi, ax].sum(axis=1))              # nang luong tung dai

    # === 4 pho cua do lon: 12 phep ===
    for g in cfg.SIGNALS_SPEC_MAG:
        s = spectrum(np.sqrt((win[g] ** 2).sum(axis=2)))
        m = s.mean(axis=1, keepdims=True)
        sd = s.std(axis=1) + 1e-30
        add('f_%s_mag_mean' % g, s.mean(axis=1))
        add('f_%s_mag_std' % g, s.std(axis=1))
        add('f_%s_mag_mad' % g, mad(s))
        add('f_%s_mag_max' % g, s.max(axis=1))
        add('f_%s_mag_min' % g, s.min(axis=1))
        add('f_%s_mag_energy' % g, (s * s).mean(axis=1))
        add('f_%s_mag_iqr' % g, iqr(s))
        add('f_%s_mag_entropy' % g, entropy(s))
        add('f_%s_mag_mean_freq' % g, (s * FREQS).sum(axis=1) / (s.sum(axis=1) + 1e-30))
        add('f_%s_mag_skew' % g, ((s - m) ** 3).mean(axis=1) / sd ** 3)
        add('f_%s_mag_kurt' % g, ((s - m) ** 4).mean(axis=1) / sd ** 4)

    # === 7 goc so voi huong gravity: phan biet tu the dung / ngoi / nam ===
    g_mean = win['gravity'].mean(axis=1)
    for g in ('body_acc', 'body_acc_jerk', 'gyro', 'gyro_jerk'):
        add('angle_%s_gravity' % g, angle_to(win[g].mean(axis=1), g_mean))
    for ax, c in enumerate(cfg.CHANNELS):
        e = np.zeros_like(g_mean)
        e[:, ax] = 1.0
        add('angle_%s_gravity' % c, angle_to(e, g_mean))

    X = np.column_stack(cols)
    assert X.shape[1] == cfg.N_FEATURES_ALL, X.shape[1]
    return names, X


def feature_names():
    """552 ten, lay tu chinh extract_features() nen khong the lech voi gia tri."""
    zero = {g: np.zeros((1, cfg.WINDOW, 3)) for g in cfg.SIGNALS}
    return extract_features(zero)[0]


def pipeline(acc, gyro):
    """Raw acc/gyro (N,3) -> ma tran dac trung (n_win, 552). None neu qua ngan."""
    sig = preprocess(acc, gyro)
    win = {}
    for name, s in sig.items():
        w = make_windows(s)
        if w is None:
            return None
        win[name] = w
    return extract_features(win)[1]
