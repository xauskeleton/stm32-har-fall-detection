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
# Duong tien xu ly: median 3 -> lowpass 20 Hz -> tach gravity 0.3 Hz.
# Dung pipeline chinh thuc cua UCI HAR. Median + lowpass co ich vi bo dac trung co
# JERK (dao ham), ma dao ham khuech dai nhieu tan so cao ti le voi tan so.
# Xem docs/DECISIONS.md muc 4.
FILTER_ORDER = 3         # Butterworth bac 3, dung cho ca 2 bo loc
MEDIAN = 3               # cua so median, dung cho CA dac trung VA kenh phat hien soc
FC_NOISE = 20.0          # Hz - lowpass khu nhieu, CHI cho duong dac trung
FC_GRAV = 0.3            # Hz - lowpass tach gravity

# === Bo dac trung ===
# 561 dac trung tinh ra, roi giu 160 cot do RFE chon (docs/DECISIONS.md muc 2).
# Thu tu PHAI giu nguyen y nhu har/dsp.py sinh ra - firmware doc theo dung thu tu
# nay, dao thu tu la model sai im lang. dsp.feature_names() de kiem tra.

# 5 nhom tin hieu 3 truc. gravity = lowpass 0.3 Hz cua acc, body_acc = acc - gravity,
# jerk = dao ham theo thoi gian.
SIGNALS = ('body_acc', 'gravity', 'body_acc_jerk', 'gyro', 'gyro_jerk')

# Nhom nao duoc lay them pho FFT (3 truc). gravity va gyro_jerk khong can:
# gravity gan nhu hang so, gyro_jerk lay pho chi lap lai thong tin cua gyro.
SIGNALS_SPEC = ('body_acc', 'body_acc_jerk', 'gyro')

# Nhom nao duoc lay pho cua DO LON (magnitude)
SIGNALS_SPEC_MAG = ('body_acc', 'body_acc_jerk', 'gyro', 'gyro_jerk')

CHANNELS = ('x', 'y', 'z')

# Nhom gan nhu HANG SO. Cac phep entropy / arCoeff / correlation tren chung MAT DIEU
# KIEN SO: tinh trong float32 ra ket qua khac float64 tan 5.8e18 (Burg) hoac 56%
# (entropy). Firmware chay float32 nen nhung cot do khong dung duoc -> bo han.
SIGNALS_NEAR_CONST = ('gravity',)

# Cac phep tinh va thu tu cua chung nam ngay trong har/dsp.py extract_features(),
# ten dac trung sinh cung luc voi gia tri nen khong the lech nhau.
# Kenh do lon khong co 'sma': do lon luon >= 0 nen mean(|x|) trung khop mean(x) tung bit
# (ban 561 chinh thuc cua HAPT co ca hai nen thua 9 cot; o day bo di -> 552).

N_SPECTRUM = 64          # bin 1..64 cua FFT 128 diem, BO bin DC
SPEC_SCALE = 64.0        # chia pho cho 64 -> bien do 1 phia

# 14 dai tan cho bandsEnergy, tinh theo so hieu bin (1..64, tinh tu 1)
BANDS = ((1, 8), (9, 16), (17, 24), (25, 32), (33, 40), (41, 48), (49, 56), (57, 64),
         (1, 16), (17, 32), (33, 48), (49, 64), (1, 24), (25, 48))

ENTROPY_BINS = 256       # histogram 256 o CO DINH tren [0,1], xem dsp.entropy()
AR_ORDER = 4             # bac cua mo hinh AR (Burg)

N_FEATURES_ALL = 516     # tong so dac trung tinh ra
N_FEATURES = 160         # so dac trung giu lai sau RFE -> vao model

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

# === Chia train/test theo NGUOI, khong theo mau ===
# Dung dung split chinh thuc cua HAPT. Chia ngau nhien theo cua so la SAI:
# cua so chong nhau 50% nen ban gan trung nhau se nam o ca hai ben.
TRAIN_USERS = frozenset({1, 3, 5, 6, 7, 8, 11, 14, 15, 16, 17, 19,
                         21, 22, 23, 25, 26, 27, 28, 29, 30})
TEST_USERS = frozenset({2, 4, 9, 10, 12, 13, 18, 20, 24})

# === Model ===
# LogisticRegression 160 -> 12: 1932 tham so = 7.5 KB float32.
# Ly do chon (thang ca XGBoost, SVM RBF va Random Forest): docs/DECISIONS.md muc 2.
LOGREG_C = 0.1
SEED = 42
