"""
Hang so dung chung cho ca pipeline Python va firmware C.

Ban sao C nam o firmware/Core/Inc/model_params.h, do scripts/export_model.py sinh ra.
Sua o day roi chay lai export_model.py, KHONG sua tay file C.
"""

# === Lay mau ===
FS = 50                  # Hz
WINDOW = 128             # 128 mau = 2.56 s
STEP = 64                # overlap 50% -> phan loai moi 1.28 s

# === Loc ===
FILTER_ORDER = 3         # Butterworth bac 3
FC_GRAV = 0.3            # Hz - lowpass tach gravity
MEDIAN_SHOCK = 3         # cua so median, CHI cho kenh phat hien soc gia toc

# === 120 dac trung: 3 nhom x 4 kenh x 10 dac trung ===
# Thu tu PHAI giu nguyen: for group: for channel: for feature.
# Firmware doc theo dung thu tu nay, dao thu tu la model sai im lang.
GROUPS = ('total_acc', 'body_acc', 'gyro')
CHANNELS = ('x', 'y', 'z', 'mag')
TIME_FEATURES = ('mean', 'std', 'max', 'min', 'rms', 'mad')
FREQ_FEATURES = ('fft_max', 'fft_mean', 'fft_energy', 'mean_freq')

N_FEATURES = len(GROUPS) * len(CHANNELS) * (len(TIME_FEATURES) + len(FREQ_FEATURES))

# === Nhan hoat dong (giu nguyen ID cua HAPT) ===
ACTIVITY_LABELS = {
    1: 'WALKING', 2: 'WALKING_UPSTAIRS', 3: 'WALKING_DOWNSTAIRS',
    4: 'SITTING', 5: 'STANDING', 6: 'LAYING',
    7: 'STAND_TO_SIT', 8: 'SIT_TO_STAND', 9: 'SIT_TO_LIE',
    10: 'LIE_TO_SIT', 11: 'STAND_TO_LIE', 12: 'LIE_TO_STAND',
}
N_CLASSES = len(ACTIVITY_LABELS)

# Nhom nhan cho state machine phat hien te nga
LABEL_LAYING = 6
LABELS_ACTIVE = frozenset({1, 2, 3, 4, 5})          # chua nam
LABELS_UPRIGHT = frozenset({1, 2, 3, 5})            # dung / di bo
LABELS_LIE_DOWN_TRANSITION = frozenset({9, 11})     # SIT_TO_LIE, STAND_TO_LIE
LABELS_GET_UP_TRANSITION = frozenset({10, 12})      # LIE_TO_SIT, LIE_TO_STAND

# === Chia train/test theo NGUOI, khong theo mau ===
TRAIN_USERS = frozenset({1, 3, 5, 6, 7, 8, 11, 14, 15, 16, 17, 19,
                         21, 22, 23, 25, 26, 27, 28, 29, 30})
TEST_USERS = frozenset({2, 4, 9, 10, 12, 13, 18, 20, 24})

# === Model ===
# LogisticRegression 120 -> 12: 1452 tham so = 5.7 KB float32.
# Ly do chon (thang ca SVM RBF va Random Forest): docs/DECISIONS.md muc 3.
LOGREG_C = 0.1
SEED = 42
