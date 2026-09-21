"""
Hang so dung chung cho ca pipeline Python va firmware C.

Moi gia tri o day deu co ban sao trong firmware/Core/Inc/har_config.h.
Khi sua o day PHAI sua ca ben kia (scripts/export_model.py se kiem tra lai).
"""

# === Lay mau ===
FS = 50                  # Hz - tan so lay mau (giong HAPT / UCI HAR)
WINDOW = 128             # 128 mau = 2.56s @ 50Hz
STEP = 64                # overlap 50%

# === Loc median cho kenh phat hien soc gia toc ===
# Cua so 3 mau. Xoa gai nhieu don le (HAPT co gai 1.49g trong 1 mau khi nam yen)
# ma khong lam tron bien doi gia toc that. Do: tu the tinh thoa dieu kien soc
# 12.9% -> 0.0%, con di xuong cau thang (co luc dap that) van 74.9%.
# Chi dung cho kenh SOC, KHONG dung cho dac trung phan loai.
MEDIAN_SHOCK = 3

# === Butterworth ===
FILTER_ORDER = 3
FC_NOISE = 20.0          # Hz - lowpass cat nhieu cao tan
FC_GRAV = 0.3            # Hz - lowpass tach gravity

# === Feature layout ===
GROUPS = ('total_acc', 'body_acc', 'gyro')          # 3 nhom tin hieu
CHANNELS = ('x', 'y', 'z', 'mag')                   # 4 tin hieu / nhom
TIME_FEATURES = ('mean', 'std', 'max', 'min', 'rms', 'mad')
FREQ_FEATURES = ('fft_max', 'fft_mean', 'fft_energy', 'mean_freq')

N_FEATURES = len(GROUPS) * len(CHANNELS) * (len(TIME_FEATURES) + len(FREQ_FEATURES))   # 120
N_FEATURES_TIME_ONLY = len(GROUPS) * len(CHANNELS) * len(TIME_FEATURES)                # 72

# === Nhan hoat dong (giu nguyen ID cua HAPT) ===
ACTIVITY_LABELS = {
    1: 'WALKING', 2: 'WALKING_UPSTAIRS', 3: 'WALKING_DOWNSTAIRS',
    4: 'SITTING', 5: 'STANDING', 6: 'LAYING',
    7: 'STAND_TO_SIT', 8: 'SIT_TO_STAND', 9: 'SIT_TO_LIE',
    10: 'LIE_TO_SIT', 11: 'STAND_TO_LIE', 12: 'LIE_TO_STAND',
}
N_CLASSES = len(ACTIVITY_LABELS)

# Nhom nhan dung cho state machine phat hien te nga
LABEL_LAYING = 6
LABELS_ACTIVE = frozenset({1, 2, 3, 4, 5})          # dang dung/ngoi/di - tu the "chua nam"
LABELS_UPRIGHT = frozenset({1, 2, 3, 5})            # dung / di bo
LABELS_LIE_DOWN_TRANSITION = frozenset({9, 11})     # SIT_TO_LIE, STAND_TO_LIE - nam xuong CHU DONG
LABELS_GET_UP_TRANSITION = frozenset({10, 12})      # LIE_TO_SIT, LIE_TO_STAND - tu nam ngoi day

# === Chia train/test theo subject (khong theo sample) ===
TRAIN_USERS = frozenset({1, 3, 5, 6, 7, 8, 11, 14, 15, 16, 17, 19,
                         21, 22, 23, 25, 26, 27, 28, 29, 30})
TEST_USERS = frozenset({2, 4, 9, 10, 12, 13, 18, 20, 24})

# === Model trien khai tren MCU ===
# CHI ML CO DIEN, khong mang no-ron.
# LogisticRegression 120 -> 12: 12*120 + 12 = 1452 tham so = 5.7 KB float32.
# (SVM RBF cua repo goc: 2636 support vector x 120 float = ~1.2 MB -> khong vua flash.)
CLASSIFIER = 'logreg'
LOGREG_C = 0.1

SEED = 42


# === Chon nhom tin hieu (quyet dinh 2026-09-21: uu tien cau hinh nhe) ===
# GROUPS_FULL : can loc Butterworth 0.3Hz de tach gravity -> firmware phai co biquad
# GROUPS_MIN  : KHONG can bat ky bo loc nao -> firmware khong co biquad,
#               va khong con van de lech train/deploy giua filtfilt va sosfilt
GROUPS_FULL = ('total_acc', 'body_acc', 'gyro')
GROUPS_MIN = ('total_acc', 'gyro')

# Lowpass 20Hz mac dinh TAT: ablation cho thay chi them ~0.5% accuracy,
# va co truong hop con lam TE HON (xem docs/DECISIONS.md muc 1).
USE_BUTTER = False


def n_features(groups=GROUPS_FULL, use_fft=True):
    """So dac trung cua mot cau hinh."""
    per_ch = len(TIME_FEATURES) + (len(FREQ_FEATURES) if use_fft else 0)
    return len(groups) * len(CHANNELS) * per_ch
