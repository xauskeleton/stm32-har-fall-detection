# Quyết định kỹ thuật — số liệu chứng minh

## Cách đọc các con số trong tài liệu này

Chia **train / test theo NGƯỜI**, dùng đúng split chính thức của HAPT: 21 người train
(7.767 cửa sổ), 9 người test (3.162 cửa sổ). Seed 42.

- **TEST** = con số báo cáo, chấm sau khi đã chốt cấu hình.
- Các **bảng so sánh phương án** trong tài liệu này đo bằng **cross-validation trong tập
  train** (7 fold, chia theo người) — không phải một tập thứ ba, chỉ là cách so các phương án
  mà không phải chạm vào tập test. Ghi `trung bình ± std`.

Độ lệch giữa người là **±2…3%**, nên chênh lệch nhỏ hơn mức đó không phân biệt được với việc
rút thăm được người dễ. Vì vậy bảng nào cũng có cột **"thắng n/7 fold"** — chỉ ≥ 6/7 mới coi
là bằng chứng.

Chỉ số quyết định, theo đúng thứ tự này: **F1 của 4 nhãn chuyển tư thế liên quan đến nằm**
(9–12, bộ phát hiện té ngã sống nhờ chúng) → **macro-F1** → **accuracy**. Chi phí flash/RAM/thời
gian là **ràng buộc**, không phải mục tiêu.

Không bao giờ chia ngẫu nhiên theo cửa sổ. Cửa sổ chồng nhau 50% nên bản gần trùng nhau sẽ
nằm ở cả hai bên:

| cách chia (7 fold) | accuracy | macro-F1 | F1 4 nhãn ngã |
|---|---|---|---|
| ngẫu nhiên theo cửa sổ | 95,53% **±0,64** | 86,59 | 72,88 |
| ngẫu nhiên phân tầng theo lớp | 95,56% **±0,27** | 87,10 | 73,71 |
| theo đoạn | 94,78% ±1,88 | 83,99 | 70,04 |
| **theo người (đúng)** | 93,71% **±2,15** | 85,55 | 72,55 |

Accuracy bị thổi +1,8% — có thật nhưng vừa phải. Thứ bị bóp méo nặng hơn là **độ lệch chuẩn**:
±0,27 so với ±2,15. Chia sai làm ta tin mình phân biệt được chênh lệch 0,5%, và đó là cách
chắc chắn nhất để tinh chỉnh vào nhiễu.

---

## 0. Nguồn dữ liệu — đọc mục này trước

`data_raw/RawData` phải là **bản gốc từ UCI**. Bản dùng trước đây là HAPT đã **cộng nhiễu
Gauss σ ≈ 0,30** (phục vụ bài tập khử nhiễu môn Xử lý tín hiệu số), và **mọi số liệu trong
các phiên bản trước của tài liệu này đều đo trên bản đó**.

Dấu hiệu nhận ra bản bị cộng nhiễu, đo trên một cửa sổ đang **đứng yên**:

| | bản cộng nhiễu | bản gốc UCI |
|---|---|---|
| std dư mỗi trục acc | **0,294 / 0,304 / 0,299** g | **0,022 / 0,020 / 0,022** g |
| std dư gyro | 0,300 / 0,289 / 0,286 rad/s | — |
| giá trị phân biệt trong 1.000 mẫu | 1.000 (liên tục, 16 chữ số) | **236** (lượng tử 1/720 g) |
| phần dư sau khi bỏ < 2 Hz | độ nhọn **3,14**, độ lệch −0,11 → Gauss | — |
| tương quan chéo giữa các trục và giữa 2 cảm biến | 0,01…0,04 → độc lập | — |

Hai bằng chứng quyết định: nhiễu acc **0,28 g** và nhiễu gyro **0,29 rad/s** — hai đơn vị vật lý
khác nhau mà cùng một con số, không cơ chế vật lý nào làm được; và dữ liệu cảm biến thật phải
bị **lượng tử hoá** bởi ADC, không thể là số thực liên tục 16 chữ số.

Ảnh hưởng: cùng 120 đặc trưng, cùng 1.452 tham số, chỉ đổi dữ liệu:

| | nhiễu σ=0,30 | gốc UCI |
|---|---|---|
| accuracy (TEST) | 86,88% | **92,82%** |
| macro-F1 | 80,69 | **84,47** |
| báo động sai té ngã | 2 / 372 phút | **0** |

Mức tăng nằm gần hết ở 6 hoạt động cơ bản (SITTING 0,79→0,88, STANDING 0,82→0,90,
WALKING 0,86→0,95). **6 lớp chuyển tư thế gần như không đổi** (+0,01…+0,02, hai lớp còn giảm).

Sau khi thay dữ liệu **phải xoá `data_raw/hapt_raw.npz`** — cache giữ tín hiệu cũ và không
báo lỗi gì.

---

## 1. Phát hiện té ngã từ nhãn đầu ra, không thêm lớp thứ 13

HAPT không có nhãn FALL, nhưng 12 nhãn của nó đã đủ phân biệt:

```
Nằm xuống chủ động :  STANDING -> STAND_TO_LIE -> LAYING
                      SITTING  -> SIT_TO_LIE   -> LAYING
Té ngã             :  WALKING/STANDING -> (không có nhãn transition) -> LAYING
                      kèm một cú sốc gia tốc
```

Đo bằng `scripts/eval_fall.py` trên **toàn bộ 61 experiment liên tục (372,3 phút,
17.453 cửa sổ)**. HAPT không có té ngã nên **mọi** sự kiện FALL đều là báo nhầm.

| nguồn nhãn | báo nhầm |
|---|---|
| `oracle` (nhãn thật từ `labels.txt`) | **0** / 372 phút |
| `model` (nhãn do bộ phân loại sinh ra) | **0** / 372 phút |

Trên **118 lần nằm xuống chủ động**, `voluntary_guard` chặn đúng toàn bộ.

### `upright_min` — muốn ngã thì trước đó phải đứng thật

Model 160 đặc trưng ban đầu cho **3 báo nhầm** dù mọi chỉ số mức cửa sổ đều tốt hơn model 120.
Nhãn `oracle` vẫn 0, nên lỗi ở bộ phân loại chứ không ở thiết kế state machine. Cả 3 cùng
một dạng — người đang nằm, bộ phân loại đoán sai **một cửa sổ**:

```
LIE_TO_SIT   SITTING   LAYING LAYING
LIE_TO_STAND WALKING   LAYING LAYING
LAYING       STANDING  LAYING LAYING
```

`_abrupt_to_laying()` cũ chỉ xem nhãn ngay trước, nên một cửa sổ đoán sai là đủ báo động.
Thêm `upright_min`: phải ở tư thế chưa nằm **liên tiếp ít nhất 3 cửa sổ** (~3,8 s):

| `upright_min` | báo nhầm / 21 người train | báo nhầm / 9 người test |
|---|---|---|
| 1 (hành vi cũ) | 3 / 265 phút | 1 / 108 phút |
| 2 | 1 / 265 phút | 0 |
| **3** | **0** | **0** |
| 4 | 0 | 0 |

Chọn `3` trên người train, người test xác nhận lại. Độ nhạy **không mất**: nhánh sốc gia tốc
chạy độc lập với nhánh chuỗi nhãn, kiểm bằng chuỗi giả lập thì ngay cả trường hợp chỉ đứng
1 cửa sổ trước khi ngã vẫn bắt được — vì cú sốc tự kích hoạt trạng thái SHOCK.

Bài học chung: **F1 mức cửa sổ không phải chỉ số triển khai.** Model 160 thắng ở cả accuracy,
macro-F1 và F1 4 nhãn ngã nhưng vẫn làm state machine đầu-cuối xấu đi. Phải đo đầu-cuối.

`har/fall.py` và `firmware/Core/Src/fall_detect.c` là bản sao của nhau.

---

## 2. LogisticRegression — chỉ ML cổ điển, không mạng nơ-ron

Quét 19 cấu hình, CV 7 fold theo người. Ràng buộc flash: firmware đã dùng 161,7 / 512 KB nên
model phải dưới ~350 KB.

| model | F1 4 nhãn ngã | macro-F1 | accuracy | flash | thắng |
|---|---|---|---|---|---|
| **logreg C=0,1 (đã chọn)** | 69,44 ±7,01 | 83,48 ±4,17 | 93,04 ±2,62 | **6,6 KB** | — |
| logreg C=0,03 | 70,45 ±7,39 | 83,42 | 93,12 | 6,6 KB | 4/7 2/7 3/7 |
| logreg C=0,3 | 67,46 | 82,71 | 92,79 | 6,6 KB | 1/7 2/7 1/7 |
| logreg C=1 | 66,97 ±10,28 | 82,36 | 92,83 | 6,6 KB | 2/7 1/7 2/7 |
| logreg `balanced` | 65,36 ±10,43 | 81,53 | 92,54 | 6,6 KB | 1/7 2/7 1/7 |
| LinearSVC C=0,01 | 68,49 ±11,75 | 82,42 | 93,46 | 6,6 KB | 5/7 2/7 4/7 |
| LinearSVC C=0,1 | 68,70 | 83,59 | 93,40 | 6,6 KB | 3/7 4/7 5/7 |
| RidgeClassifier | 66,83 ±11,60 | 79,66 | 91,76 | 6,6 KB | 4/7 1/7 1/7 |
| LDA | 68,71 ±11,14 | 82,78 | 92,43 | 6,6 KB | 4/7 4/7 3/7 |
| GaussianNB | 53,82 | 68,53 | 81,67 | 11,2 KB | 0/7 |
| DecisionTree d=12 | 50,23 | 68,25 | 83,42 | 3,1 KB | 0/7 |
| RandomForest 30 d=10 | 65,37 | 78,15 | 89,03 | 87,8 KB | 3/7 0/7 0/7 |
| XGBoost 50 d=3 | 66,98 | 81,25 | 91,83 | 52,1 KB | 3/7 3/7 2/7 |
| **XGBoost 200 d=4** | **74,29 ±6,35** | **84,33 ±2,44** | 92,62 | 225,0 KB | **6/7 5/7** 2/7 |
| RandomForest 100 | 72,18 ±5,49 | 82,19 | 90,02 | 428,3 KB ✗ | 5/7 3/7 0/7 |
| SVM RBF C=10 | 72,47 ±7,15 | 84,06 | 92,91 | 649,4 KB ✗ | 5/7 5/7 4/7 |
| ExtraTrees 100 | 72,68 ±3,34 | 82,52 | 90,34 | 1.134,6 KB ✗ | 5/7 3/7 0/7 |
| KNN k=5 | 67,92 | 79,15 | 89,09 | 3.123,3 KB ✗ | 4/7 1/7 1/7 |

Ba nhận xét:

1. **Không phải mọi model cây/SVM đều vượt flash.** XGBoost 200 cây × sâu 4 chỉ **225 KB**
   (28.794 node × 8 byte), cộng firmware 161,7 KB = 387 / 512 KB — vừa. RandomForest,
   ExtraTrees, SVM RBF và KNN thì không vừa.
2. **XGBoost là model duy nhất vượt được `logreg` ở chỉ số quyết định** với biên đủ tin:
   F1 4 nhãn ngã 74,29 so với 69,44, thắng **6/7 fold**. Nó đổi lại 0,42% accuracy.
3. **Nhưng đặc trưng quan trọng hơn model.** `logreg` + bộ đặc trưng ở mục 3 cho F1 4 nhãn ngã
   **75,14**, cao hơn XGBoost + bộ đặc trưng cũ (74,29) — mà vẫn giữ 6,6 KB, vẫn là một phép
   nhân ma trận 12×160 giải thích được trong một câu, và vẫn cho `predict_proba` để đặt ngưỡng
   tin cậy trên MCU.

**→ `logreg C=0,1`, 160 → 12: 1.932 tham số = 7,5 KB float32, 1.920 MAC mỗi cửa sổ.**

Bài toán lồi nên nghiệm duy nhất, không phụ thuộc seed. Quét C thì accuracy phẳng từ 0,1 trở
lên nhưng F1 4 nhãn ngã **giảm** khi C tăng (69,44 ở C=0,1 xuống 66,97 ở C=1). Lý do phụ để
regularize mạnh: model train trên gia tốc kế điện thoại nhưng sẽ chạy trên ICM-20948 ở vị trí
gắn khác — nén mạnh hơn thì chịu lệch phân bố tốt hơn.

---

## 3. Bộ đặc trưng: tính 516, giữ 160

### Dựng lại bản đặc tả 561 của HAPT

Nhóm tác giả HAPT **không công bố** code sinh 561 đặc trưng; mọi bản dựng lại công khai đều chỉ
đọc `X_train.txt` có sẵn. Đặc tả được dựng lại bằng cách so từng cột của `X_test.txt` với đặc
trưng tự tính từ `Inertial Signals/` của bộ UCI HAR gốc (chính là tín hiệu **đã lọc của tác
giả**). `X_*.txt` chuẩn hoá min-max theo cột, mà min-max là phép affine tăng dần, nên
**|r| = 1,000000 nghĩa là công thức khớp tuyệt đối**.

`har/dsp.py` đạt **|r| = 1,000000 ở cả 15 phép**: `mad`, `iqr`, `entropy`, `arCoeff` ×4,
`correlation`, và 8 phép miền tần số. Ba chỗ `features_info.txt` nói mơ hồ, đo ra được:

| chỗ mơ hồ | đáp án |
|---|---|
| phổ "64 bin" là bin nào | **bin 1…64, BỎ bin DC**, và **chia 64**. Dùng bin 0…63 thì mọi phép miền tần số chỉ đạt ~0,999 |
| `mad` | độ lệch tuyệt đối **trung bình**, không phải median (0,966 → 1,000) |
| `iqr` | quy ước MATLAB `(i−0,5)/n`, bằng `numpy` `method='midpoint'` khi n=128 |
| `meanFreq` | trọng số `\|X\|`, không phải `\|X\|²` (0,769 → 1,000) |
| `maxInds` | `argmax` trên 64 bin, chỉ số đếm từ 1 |
| `bandsEnergy` | **tổng** `\|X\|²` trên bin lo…hi, không lấy trung bình |
| `arCoeff` | Burg bậc 4, trả `a[1..4]` |
| **`entropy`** | hàm **`entropy()` của MATLAB Image Processing Toolbox**: histogram **256 ô CỐ ĐỊNH** trên `[0,1]`, ô = `round(x·255)` **ép về biên**, rồi `−Σ p·log₂ p` |

`entropy` là chỗ không ai dựng lại được. Hàm đó dành cho ảnh nên giả định dữ liệu trong `[0,1]`
và dùng ô cố định → **không bất biến theo tỉ lệ** (tương quan 0,92 với `log(std)`). Entropy phổ,
cách ai cũng thử đầu tiên, chỉ đạt |r| = 0,49. Đây cũng là lý do phổ **phải chia 64** trước khi
đưa vào `entropy`: không chia thì mọi giá trị > 1 và bị ép hết vào một ô, |r| tụt còn 0,028.

### 516 → RFE giữ 160

Bộ của repo là **516**, khác bản 561 chính thức ở **45 cột**, cả hai lý do đều đo được:

| bỏ gì | số cột | vì sao |
|---|---|---|
| `sma` của các kênh magnitude | 9 | magnitude luôn ≥ 0 nên `mean(\|x\|)` **trùng khít từng bit** với `mean(x)`. Tìm ra bằng cách quét cặp cột có \|r\| = 1,000000 |
| `entropy`, `arCoeff`, `correlation` của nhóm `gravity` | 23 | không tính lại được trong float32 (mục 7) |
| `max_ind` của mọi nhóm | 13 | `argmax` là đầu ra rời rạc, lệch 1e-7 là nhảy bin (mục 7) |

Đường cong RFE, CV 7 fold theo người, **RFE chạy riêng trong từng fold**:

| k | tham số | flash | F1 4 nhãn ngã | macro-F1 | accuracy | thắng so với k=516 |
|---|---|---|---|---|---|---|
| 516 | 6.204 | 24,2 KB | 73,33 ±7,80 | 86,27 ±3,25 | 94,24 ±2,51 | — |
| 300 | 3.612 | 14,1 KB | 74,56 ±7,34 | 87,04 ±2,84 | 94,55 ±2,20 | 4/7 6/7 5/7 |
| 200 | 2.412 | 9,4 KB | 73,24 ±4,87 | 86,59 ±2,34 | 94,51 ±2,20 | 3/7 4/7 5/7 |
| **160 (đã chọn)** | **1.932** | **7,5 KB** | **75,32** ±7,01 | **87,73** ±2,84 | **94,69** ±1,92 | 4/7 **7/7** 5/7 |
| 120 | 1.452 | 5,7 KB | 73,67 ±7,48 | 86,51 ±3,97 | 94,43 ±2,19 | 5/7 5/7 3/7 |
| 80 | 972 | 3,8 KB | 71,31 ±9,32 | 85,07 ±4,49 | 93,38 ±2,93 | 4/7 3/7 2/7 |
| 40 | 492 | 1,9 KB | 67,79 ±3,81 | 83,18 ±2,79 | 93,03 ±2,07 | 2/7 1/7 1/7 |
| 20 | 252 | 1,0 KB | 57,65 ±8,97 | 73,62 ±6,12 | 89,21 ±4,31 | 0/7 0/7 0/7 |

**Cắt bớt đặc trưng làm model tốt hơn, không tệ hơn.** Dùng cả 516 cho 73,33 F1 ngã;
giữ 160 cho **75,32** — hơn 2 điểm dù dùng **ít hơn 3,2 lần** tham số. 7.767 mẫu train trên
516 chiều thì logreg bắt đầu khớp nhiễu dù C = 0,1 đã nén mạnh; RFE bỏ đúng các chiều chỉ
mang nhiễu.

Chọn **k = 160**: cao nhất ở cả ba chỉ số, thắng **7/7 fold** ở macro-F1, và có độ lệch chuẩn
accuracy nhỏ nhất (±1,92) tức ổn định nhất giữa các người. Vùng 120–300 thực chất **không
phân biệt được** (mọi chênh lệch nằm trong ±2…7 độ lệch chuẩn của chính chúng); nếu cần gọn
hơn thì k = 120 (5,7 KB) là lựa chọn có số liệu đỡ lưng, mất ~1,6 F1 ngã. Dưới 80 thì sụp thật:
k = 40 mất 7,5 F1 ngã (2/7 fold), k = 20 mất 17,7 (0/7).

RFE chạy **chỉ trên tập train**. Danh sách cột là **số liệu phụ thuộc dữ liệu** — nằm trong
`models/selected.json`, được `scripts/export_model.py` sinh sang C, không viết tay vào
`har/config.py`.

### 160 cột được chọn nói gì

| nhóm tín hiệu | tính ra | được chọn | tỉ lệ |
|---|---|---|---|
| **`gravity` 3 trục** | 22 | **18** | **82%** |
| **`gravity` magnitude** | 7 | **6** | **86%** |
| `gyro` 3 trục | 40 | 25 | 63% |
| `body_acc_jerk` 3 trục | 40 | 21 | 53% |
| `body_acc` 3 trục | 40 | 16 | 40% |
| `gyro_jerk` 3 trục | 40 | 15 | 38% |
| phổ `gyro` / `body_acc` / `body_acc_jerk` | 228 | 35 | 15% |
| các kênh magnitude khác + góc | 99 | 24 | 24% |

Chia theo miền: **110 thời gian / 47 tần số / 3 góc**.

| phép tính | tính ra | chọn | tỉ lệ |
|---|---|---|---|
| **`correlation`** | 12 | **12** | **100%** |
| `entropy` | 29 | 20 | 69% |
| `kurtosis` | 13 | 9 | 69% |
| `mean_freq` | 13 | 8 | 62% |
| `skewness` | 13 | 6 | 46% |
| `max` / `mean` / `min` | 99 | 39 | 39% |
| `arCoeff` (Burg) | 64 | 24 | 38% |
| `std` / `energy` / `iqr` / `mad` | 132 | 31 | 23% |
| **`bandsEnergy`** | **126** | **7** | **6%** |

Ba điều đáng ghi:

1. **`gravity` là nhóm đắt giá nhất tính theo mật độ** — 82% số cột của nó được chọn, gấp đôi
   mọi nhóm khác, dù nó đã bị cắt mất 3 phép. Khớp với mục 4: bỏ tách gravity mất 8,55 F1 ngã.
2. **`correlation` được chọn 12/12.** Không phép nào khác đạt 100%. Tương quan giữa các trục mô
   tả *phối hợp chuyển động*, thứ mà thống kê trên từng trục riêng lẻ không thấy.
3. **`bandsEnergy` gần như là gánh nặng chết:** 126 cột tính ra (24% bộ đặc trưng) chỉ dùng 7.
   Năm trong tám dải tần không có cột nào được chọn — thông tin trên 12,5–25 Hz gần như vô dụng,
   hợp lý vì chuyển động người tập trung dưới 10 Hz. Chi phí tính không đáng kể (dùng lại phổ đã
   có) nên chưa bỏ, nhưng đây là chỗ cắt được 119 cột nếu cần gọn.

### Không bỏ được gì khỏi bộ 120 cũ

Để đối chiếu, ablation trên bộ 120 cũ (dữ liệu sạch, CV 7 fold):

| cấu hình | n | F1 4 nhãn ngã | macro-F1 | accuracy | thắng |
|---|---|---|---|---|---|
| **120 đầy đủ** | 120 | **69,44** | **83,48** | **93,04** | — |
| chỉ miền thời gian | 72 | 69,01 | 81,53 | 90,09 | 3/7 1/7 1/7 |
| chỉ miền tần số | 48 | **47,87** | 69,95 | 89,17 | 0/7 |
| bỏ kênh magnitude | 90 | 69,25 | 82,72 | 92,51 | 3/7 2/7 0/7 |
| bỏ gyro | 80 | 65,46 ±10,03 | 78,84 | 89,09 | 3/7 0/7 0/7 |
| bỏ `body_acc` | 80 | 68,55 | 81,22 | 92,46 | 4/7 3/7 1/7 |
| bỏ `total_acc` | 80 | 70,26 | 78,15 | 83,91 | 3/7 0/7 0/7 |
| chỉ `total_acc` | 40 | 41,75 | 61,68 | 87,32 | 0/7 |
| chỉ `body_acc` | 40 | 66,54 | 69,14 | 73,88 | 0/7 |
| chỉ `gyro` | 40 | 57,39 | 64,40 | 70,16 | 0/7 |

Miền tần số một mình sụp F1 ngã xuống 47,87 — 4 đặc trưng FFT không tự đứng được, nhưng thêm
vào 72 đặc trưng thời gian thì kéo macro-F1 +1,95 và accuracy +2,95. Cả hai miền đều cần.

### Mốc không lấy được

Bộ 561 **chính thức** cho 93,86% với cùng pipeline, còn bộ tự tính chỉ 87,89% (trên dữ liệu
nhiễu). Chênh lệch đó **không phải do công thức** — 6% nằm gần hết ở SITTING (−13,5 F1) và
STANDING (−12,4), hai lớp chỉ phân biệt được bằng dao động thân người biên độ 0,02–0,06 rad/s,
bị nhiễu 0,30 chôn hoàn toàn. Trên dữ liệu sạch khoảng cách còn **~1 điểm** (94,56% so với 93,86%).

Ngoài ra `X_*.txt` chuẩn hoá min-max bằng hằng số **không được công bố**, nên bộ 561 chính thức
không bao giờ triển khai được. Điều này không quan trọng: tự tính đặc trưng rồi tự train scaler.

---

## 4. Tiền xử lý: median 3 → lowpass 20 Hz → tách gravity 0,3 Hz

Ba bước, bằng chứng cho mỗi bước khác nhau. Median 3 và lowpass 20 Hz là **khử nhiễu**;
lowpass 0,3 Hz là **tách tín hiệu** — nó định nghĩa ra `gravity` và `body_acc = acc − gravity`,
tức hai trong năm nhóm tín hiệu.

### Lowpass 20 Hz và median 3 cho đường đặc trưng — GIỮ (đảo kết luận của bản trước)

Trên **bộ 120 đặc trưng cũ**, CV 7 fold:

| tiền xử lý | F1 4 nhãn ngã | macro-F1 | accuracy | thắng |
|---|---|---|---|---|
| **không lọc thêm** | **69,44** | **83,48** | **93,04** | — |
| median 3 | 69,23 | 82,66 | 92,01 | 4/7 2/7 1/7 |
| lowpass 20 Hz | 69,58 | 83,09 | 92,60 | 3/7 4/7 2/7 |
| median 3 + lowpass 20 Hz | 68,52 | 82,26 | 91,64 | 3/7 2/7 1/7 |

Không có bằng chứng. Ở fs = 50 Hz, cắt tại 20 Hz chỉ chạm 1/5 trên cùng của dải.

**Nhưng trên bộ đặc trưng đầy đủ thì bảng đảo chiều** (đo trên bộ 552, tức trước khi
bỏ 36 cột không tính được trong float32 ở mục 7 — thứ tự xếp hạng không đổi):

| tiền xử lý | F1 4 nhãn ngã | macro-F1 | accuracy | thắng |
|---|---|---|---|---|
| chỉ tách gravity | 73,08 ±8,22 | 86,75 ±3,80 | 94,53 ±2,08 | — |
| + median 3 | 74,77 ±8,06 | 86,83 | 94,35 | 5/7 4/7 2/7 |
| + lowpass 20 Hz | 70,72 ±7,31 | 85,47 | 93,91 | 4/7 2/7 1/7 |
| + lowpass 10 Hz | 74,27 ±7,91 | 86,83 | 94,26 | 4/7 3/7 4/7 |
| **+ median 3 + lowpass 20 Hz** | **75,88 ±6,82** | **87,62** | **94,80** | **6/7** 5/7 4/7 |
| không tách gravity | 71,17 ±8,17 | 85,07 | 93,30 | 3/7 3/7 1/7 |
| không tách gravity + median 3 | 71,61 ±3,06 | 85,45 | 93,95 | 3/7 2/7 2/7 |
| không tách gravity + lowpass 10 Hz | 65,58 ±6,87 | 83,58 | 93,68 | 2/7 2/7 1/7 |

`median 3 + lowpass 20 Hz` — **đúng pipeline chính thức của UCI HAR, đã chọn** — thắng 6/7 fold
ở chỉ số quyết định, tốt nhất ở cả ba, và có độ lệch chuẩn nhỏ nhất. Cơ chế giải thích được: bộ 552 có
**jerk** (đạo hàm), mà đạo hàm khuếch đại nhiễu tần số cao tỉ lệ với tần số — lowpass bảo vệ
đúng chỗ đó. Bộ 120 không có jerk nên lowpass chẳng có gì để bảo vệ.

### Cái giá: phải nới ngưỡng va đập

Chọn cấu hình này thì **báo động sai đầu-cuối tăng từ 0 lên 2** (trên 372 phút HAPT) và accuracy
tập test tụt. Đo ngang hàng trên **cùng bộ 516 đặc trưng** hiện tại, chỉ khác bộ lọc:

| cấu hình | accuracy | macro-F1 | F1 4 nhãn ngã | báo động sai |
|---|---|---|---|---|
| chỉ tách gravity | **93,99%** | 85,74 | 68,02 | 0 |
| **+ median 3 + lowpass 20 Hz (đã chọn)** | 93,20% | **85,79** | **69,16** | 0 |

Tức bộ lọc đổi **0,79% accuracy** lấy **+1,14 F1 té ngã**. Lưu ý 0,79% nằm **trong nhiễu**
(±2…3% giữa người) và CV còn dự đoán bộ lọc làm accuracy *tăng* — nên thứ phân biệt được giữa hai
cấu hình là F1 té ngã, nơi bộ lọc thắng 6/7 fold.

Báo động sai đo trên 21 người train:

| đường tiền xử lý | báo động sai / 265 phút |
|---|---|
| chỉ tách gravity 0,3 Hz | 0 |
| + median 3 | 3 |
| **+ median 3 + lowpass 20 Hz (đã chọn)** | **2** |

Quét ngưỡng state machine để đưa về 0, chỉ trên người train. Không núm đơn nào đủ — tốt nhất
còn 1. Trong 27 tổ hợp `upright_min × laying_confirm × impact_g`, **14 tổ hợp cho 0, và tất cả
đều đòi `impact_g ≥ 2,2`** — tức báo động sai còn lại là một cú sốc có `a_max` nằm giữa 2,0 và
2,2 g. Chọn tổ hợp giữ độ trễ phát hiện nhỏ nhất:

| | cũ | mới |
|---|---|---|
| `impact_g` | 2,00 g | **2,20 g** |
| `upright_min` | 3 | **4** (~5,1 s) |
| báo động sai (train / test) | 2 / 2 | **0 / 0** |
| độ trễ phát hiện | 1 cửa sổ | **1 cửa sổ** (không đổi) |

**Đây là đánh đổi chưa đo được.** Nâng `impact_g` là nâng một **ngưỡng vật lý**: té ngã nhẹ hơn
2,2 g sẽ bị bỏ qua, mà không có mẫu té ngã thật nên **không thể biết tỷ lệ bỏ sót tăng bao nhiêu**
(xem mục 9). Thêm nữa median 3 làm cụt đỉnh va đập ~8% ở phân vị 99 (mục 6), nên `impact_g = 2,2`
sau median tương đương đỉnh thật ~2,4 g. Té ngã thật ở thắt lưng thường 3–6 g nên vẫn còn khoảng
dự trữ, nhưng con số 3–6 g lấy từ tài liệu, không phải đo trên thiết bị này.

Muốn quay lại `impact_g = 2,0` thì sửa một dòng trong `har/fall.py`, đổi lại nhận 2 báo động sai
trên 372 phút (1 lần / 3,1 giờ).

**Bài học vẫn giữ:** F1 mức cửa sổ **không phải** chỉ số triển khai. Cấu hình này thắng 6/7 fold
ở mức cửa sổ nhưng làm state machine xấu đi, phải nới ngưỡng mới bù lại. Trước khi chốt bất cứ
thay đổi nào ở tiền xử lý hay bộ đặc trưng, **phải chạy `eval_fall.py`**.

### Tách gravity 0,3 Hz — giữ, và đây là bằng chứng mạnh nhất trong dự án

Bỏ bộ lọc này thì không còn `gravity` lẫn `body_acc`, chỉ còn `acc`, và 7 đặc trưng góc mất
theo. Vẫn cùng 12 phép, cùng RFE giữ 160, cùng CV 7 fold, **cùng `median 3 + lowpass 20 Hz` ở
trước** (đo trên bộ 552, trước khi bỏ 36 cột ở mục 7):

| cấu hình | n tính ra | F1 4 nhãn ngã | macro-F1 | accuracy | thắng |
|---|---|---|---|---|---|
| **có lọc 0,3 Hz (đã chọn)** | 552 | **75,69** ±5,35 | **87,25** ±3,03 | **94,83** ±2,22 | — |
| không tách gravity | 493 | 67,15 ±6,38 | 83,97 ±3,91 | 93,60 ±2,85 | **0/7** 1/7 0/7 |

Bỏ đi mất **8,55 F1 té ngã**, thắng **0/7 fold** — không phép so nào khác trong dự án dứt khoát
bằng. Báo động sai đầu-cuối thì 0 ở cả hai, nên bỏ nó cũng không đổi được gì ở đó.

**Hai bộ lọc bổ trợ nhau, không độc lập.** Dưới đường tiền xử lý cũ (không median, không
lowpass 20 Hz), bỏ tách gravity chỉ mất 1,91 F1 ngã và thắng 3/7 fold — khiêm tốn. Dưới đường
mới thì mất 8,55 và thắng 0/7. Lý do: `median 3 + lowpass 20 Hz` làm `acc` mượt hơn, nên
`body_acc = acc − gravity` trở thành tín hiệu "chỉ có chuyển động" sạch hơn, và `std` / `iqr` /
`entropy` / `arCoeff` trên nó đo đúng thứ cần đo. Không tách thì thành phần tư thế bám vào mọi
phép tính.

Ghi lại một nhận định **sai** của bản trước, để không ai lặp: *"bỏ tách gravity thì không phân
biệt được NGỒI/ĐỨNG/NẰM"*. Đo thực tế ba lớp tư thế tĩnh gần như không đổi (SITTING 93,7 vs
93,4; STANDING 94,5 vs 94,3; LAYING 100 cả hai) — vì `mean(acc)` trên cửa sổ 2,56 s **tự nó đã
là hướng gravity** khi người đứng yên. Mất mát tập trung vào **4 nhãn chuyển tư thế**, không
vào tư thế tĩnh.

### Phải khởi tạo state bộ lọc, không để mặc định 0

Bộ lọc 0,3 Hz có thời gian xác lập dài. Nếu state bắt đầu từ 0 thì gravity sai rất nhiều lúc
đầu:

| t | lệch gravity so với trạng thái xác lập |
|---|---|
| 0 s | **0,918 g** |
| 1 s | 0,545 g |
| 5 s | 0,009 g |
| 10 s | 0,0001 g |

Sai > 0,01 g kéo dài **4,4 s đầu → 4 cửa sổ đầu tiên sai nhãn**. Trên HAPT thì 4/10.929 cửa sổ
không ảnh hưởng số liệu, nhưng trên board thì đó là **mỗi lần cấp điện hoặc reset**.

`har/dsp.py` khởi tạo state về trạng thái xác lập theo mẫu đầu tiên (`sosfilt_zi * acc[0]`) —
2 dòng, và **firmware phải làm y hệt**, nếu không thì Python và C lệch nhau ở 4 cửa sổ đầu.

---

## 5. Lọc causal, không dùng `filtfilt`

`filtfilt` chạy forward-backward nên cần biết cả tín hiệu tương lai → không real-time được.
Lo ngại ban đầu là train (zerophase) / deploy (causal) lệch nhau sẽ mất accuracy.

Đo bằng tương quan với 7.767 cột đặc trưng chính thức: causal đạt |r| trung bình **0,872**,
zerophase chỉ **0,715**. Causal **không hề kém hơn** — lựa chọn miễn phí, không đánh đổi gì.

Dạng SOS để trùng khớp 1-1 với cascade biquad bên firmware.

### `jerk` cũng phải causal — sai phân LÙI, không phải tiến

Bản 561 chính thức của HAPT lấy `jerk[n] = (x[n+1] − x[n]) · fs`, tức **sai phân tiến** — cần
mẫu **tương lai** `x[n+1]` để tính được mẫu `n`. Đó là non-causal, cùng loại vấn đề với
`filtfilt`, và nó chỉ lộ ra khi viết bản C streaming: pipeline streaming tự nhiên dùng sai phân
lùi, làm `jerk` **lệch 1 mẫu** so với Python → `f_body_acc_jerk_y_mean_freq` sai 1,03 (10%).

Đã đổi `har/dsp.py deriv()` sang **sai phân lùi** `jerk[n] = (x[n] − x[n−1]) · fs`, mẫu đầu = 0
— đúng công thức mà `har_pipeline.c` tính được mà không cần đệm thêm mẫu nào. Accuracy không
đổi đáng kể (93,20% → **93,23%**), và đổi lại pipeline hết non-causal hoàn toàn.

---

## 6. Lọc median 3 — chỉ cho kênh sốc, và lý do đã đổi

Phiên bản trước lập luận: nhiễu gia tốc kế 0,30 g/trục làm **12,9%** cửa sổ tư thế tĩnh thoả
điều kiện sốc, median 3 đưa về 0,0%. **Lập luận đó dựa trên nhiễu nhân tạo** (mục 0).

Đo lại trên **5.738 cửa sổ tư thế tĩnh** của dữ liệu gốc, ngưỡng `a_min ≤ 0,6g` VÀ `a_max ≥ 2,0g`:

| kênh sốc | thoả điều kiện sốc | `a_min` TB | `a_max` TB |
|---|---|---|---|
| median 3 | **0,00%** | 1,0031 | 1,0392 |
| không lọc | **0,00%** | 0,9989 | 1,0440 |

Cả hai đều 0,00%. Và median 3 **làm cụt đỉnh va đập thật** — đo trên các đoạn đi lại:

| | `a_max` TB | phân vị 99% | lớn nhất |
|---|---|---|---|
| median 3 | 1,7525 g | 2,2498 | 2,5251 |
| không lọc | **1,8298** g | **2,4335** | **2,7829** |

Trên dữ liệu sạch, median 3 chỉ còn là mất mát thuần: **−4% biên độ trung bình, −8% ở phân vị 99%**,
mà không đổi lại được gì. ICM-20948 thật có sàn nhiễu ~0,0016 g ở 50 Hz, thấp hơn Galaxy S II
khoảng 14 lần nữa, nên vấn đề càng không tồn tại.

`cfg.MEDIAN = 3` vẫn bật, và từ mục 4 thì nó dùng cho **cả** đường đặc trưng lẫn kênh sốc —
nhưng **lý lẽ ban đầu (nhiễu 0,30 g) không còn**. Lý lẽ hiện tại chỉ là: cùng một bước tiền xử lý
với đường đặc trưng, và cho 0 báo nhầm. Phải đo lại trên ICM-20948 thật rồi quyết.

Lưu ý chiều ngược lại: chính vì median 3 làm cụt đỉnh mà `impact_g` phải nâng lên 2,2 g (mục 4) —
hai thứ này bù trừ nhau, và cả hai đều chưa kiểm được trên té ngã thật.

**Kênh sốc KHÔNG dùng lowpass 20 Hz** — nó cần giữ nguyên đỉnh va đập.

---

## 7. float32 — và những đặc trưng KHÔNG tính lại được trong float32

STM32F446RET6 là Cortex-M4F — có **FPU cứng single-precision**, phép `float` chạy 1 chu kỳ.
int8 chỉ có lợi khi MCU không có FPU (M0/M3) hoặc model lớn cỡ MB; ở đây không điều kiện nào
đúng: model 7,5 KB / 512 KB flash.

### Đối chiếu C ↔ Python: 0 / 3.156 cửa sổ lệch nhãn

Firmware C đã có đủ trong `firmware/Core/`: `har_dsp.c` (median 3 streaming, cascade biquad,
FFT 128 tự viết, 14 phép thống kê), `har_features.c` (516 đặc trưng), `har_classifier.c`,
`har_pipeline.c` (ring buffer, chạy mỗi 64 mẫu mới), `fall_detect.c`. Toàn bộ `float`, biên dịch
`-Wall -Wextra -Wdouble-promotion` **0 warning**, không `malloc` trong đường chạy.

Chạy **chính code đó** trên toàn bộ tập test:

| | Python (float64) | C (float32) |
|---|---|---|
| accuracy | **91,73%** | **91,73%** |
| số cửa sổ lệch nhãn | — | **0 / 3.156** |
| sai số đặc trưng lớn nhất (golden 5 cửa sổ) | — | 0,055 |

Hai ghi chú về con số:

- **91,73% chứ không phải 93,23%**: phép đối chiếu lọc **từng đoạn riêng**, còn `train.py` lọc
  **cả bản ghi liền mạch** rồi mới cắt cửa sổ. Bộ lọc có trạng thái nên hai cách khác nhau chút;
  điều quan trọng là C và Python dùng **cùng một cách** nên so được.
- **3.156 chứ không phải 3.162**: `median 3` streaming trễ 1 mẫu (cần biết mẫu sau mới xuất được
  mẫu hiện tại), nên vài đoạn mất cửa sổ cuối. Đây là 6 cửa sổ trên 353 đoạn, và trên thiết bị
  chạy liên tục thì không tồn tại.

`firmware/test/test_parity.c` + `firmware/Makefile` chạy kiểm tra này tự động trên 5 cửa sổ
golden: `make -C firmware test`. Tiêu chí ĐẠT là **nhãn khớp từng cửa sổ**.

### 36 đặc trưng phải bỏ vì float32 không tính lại được

Kết quả đầu tiên của phép đối chiếu là **60,28% ở C so với 91,87% ở Python** — 36,5% cửa sổ lệch
nhãn. Truy ra **ba** nguyên nhân, cả ba đều là bài học đáng ghi:

**Lỗi thứ nhất — khởi tạo state bộ lọc (lỗi lập trình).** Bản C lọc từng section hết cả tín hiệu
rồi mới sang section sau, nhưng lấy `zi[s] * x[0]` trong đó `x[0]` **đã bị section trước ghi
đè**. Phải chụp mẫu đầu tiên **trước** vòng lặp section, đúng như `scipy.signal.sosfilt` làm.
Sai số gravity: **0,899 → 2,2e-05**.

**Lỗi thứ hai — đặc trưng mất điều kiện số (lỗi thiết kế bộ đặc trưng).**

| đặc trưng | sai số C ↔ Python | nguyên nhân |
|---|---|---|
| `t_gravity_*_ar*` | **5,8e18** | Burg trên tín hiệu gần hằng số: hệ số hội tụ về (1−z)⁴ nên `1 − mu²` triệt tiêu hết chữ số có nghĩa của float32 |
| `f_*_max_ind` | 9 bin | `argmax` là đầu ra **rời rạc** — hai bin gần bằng nhau thì lệch 1e-7 đủ nhảy sang bin khác |
| `t_gravity_*_entropy` | 0,37 (**56%**) | histogram có **biên cứng** `round(x·255)`; gravity gần hằng số nên gần hết mẫu dồn vào 1–2 ô, xê dịch một chút là đổi ô hàng loạt |
| `t_gravity_corr_*` | 0,14 | hiệp phương sai của tín hiệu gần hằng số là hiệu hai số gần bằng nhau → triệt tiêu |

**Lỗi thứ ba — `jerk` non-causal (lỗi thiết kế, xem mục 5).** Python dùng sai phân **tiến**
(cần mẫu tương lai), pipeline streaming dùng sai phân **lùi** → lệch 1 mẫu, làm
`f_body_acc_jerk_y_mean_freq` sai 1,03 (10%). Đã đổi Python sang sai phân lùi.

**Nguyên tắc rút ra: thứ không tính lại được trên phần cứng đích thì không được nằm trong
model.** Nếu giữ, firmware sẽ nạp giá trị 1e18 vào phép nhân ma trận — model hỏng **im lặng**,
không có thông báo lỗi nào. Đã bỏ 36 cột đó (552 → 516).

Giá phải trả, đo trên tập test: accuracy 93,26% → **93,23%**, macro-F1 86,68 → **85,81**,
F1 4 nhãn ngã 72,42 → **69,16**. Nhưng **3,3 điểm F1 ngã đó không phải mất mát thật** — nó dựa
trên đặc trưng chỉ tồn tại trong float64 của Python. Con số 72,42 là **ảo**.

### int8 thì không cần

Đo trên 3.162 cửa sổ tập test, so với `float64` của Python:

| cách lưu trọng số | kích thước W | sai số trọng số | số cửa sổ lệch nhãn |
|---|---|---|---|
| **float32** | **7,5 KB** | 2,6e-08 | **0 / 3.162 (0,000%)** |
| int16 | 3,8 KB | 1,2e-05 | 0 / 3.162 (0,000%) |
| int8 | 1,9 KB | 3,2e-03 | 2 / 3.162 (**0,063%**) |

int8 tiết kiệm 5,6 KB trên 512 KB flash (1,1%) nhưng đổi lấy 0,063% cửa sổ sai nhãn. Không đáng.
int16 miễn phí về độ chính xác nhưng tiết kiệm 3,8 KB cũng chẳng giải quyết gì.

### Điều CẦN chú ý thay vì int8

**Toàn bộ firmware phải dùng `float`, tuyệt đối không dùng `double`.** FPU của Cortex-M4 không có
double precision — compiler emulate bằng software, chậm 50–100 lần. Một biến `double` lọt vào
vòng lặp tính đặc trưng là đủ phá toàn bộ ngân sách thời gian.

Cạm bẫy trong C: `1e-10` và hằng số thập phân không có hậu tố là `double`. Phải viết `1e-10f`,
`0.5f`, và dùng `sqrtf()` / `fabsf()` / `logf()` / `log2f()` chứ không phải bản `double`.
Biên dịch với `-Wdouble-promotion` để bắt tự động.

### Ngưỡng cho test đối chiếu

`GOLDEN_TOL = 0.3` (tuyệt đối), và **tiêu chí chính là NHÃN phải khớp từng cửa sổ**, không phải
sai số đặc trưng. Con số 0,3 là số đo thật: sai số lớn nhất 0,248 ở `f_body_acc_x_kurt` — mô men
bậc 4 trên giá trị phổ ~17, tức 1,5% tương đối. Tuyệt đại đa số cột sai dưới 1e-4.

Không dùng ngưỡng tương đối: đặc trưng `mean` của `body_acc` vốn ≈ 0 theo định nghĩa nên sai số
tương đối ở đó vô nghĩa.

## 8. Phần cứng: F446RET6 + ICM-20948, ±4 g / ±1000 °/s

STM32F446RET6 (Cortex-M4F 180 MHz, 512 KB flash, 128 KB SRAM) + ICM-20948 qua I2C1 400 kHz,
địa chỉ 0x68. Lấy mẫu 50 Hz bằng TIM6. Số liệu đo thật từ linker map: xem `docs/HARDWARE.md`.

Chỉ dùng **accelerometer + gyroscope**. Bỏ magnetometer dù ICM-20948 có: HAPT thu bằng
Samsung Galaxy S II chỉ với acc + gyro, thêm từ kế vào lúc inference sẽ tạo ra đặc trưng mà
model chưa từng thấy khi train.

### HAPT đã bị bão hoà — nên không dùng nó để chọn thang đo

| | đỉnh | số lần lặp lại đúng giá trị đó |
|---|---|---|
| acc | **2,009722 g** | 11 |
| gyro | **10,008416 rad/s = 573,0 °/s** | 24 |

Cùng một giá trị lặp lại y hệt nhiều lần, trên cả 3 trục, tại đúng mốc tròn 2 g và 10 rad/s →
điện thoại được đặt ở **±2 g** và **±10 rad/s**, và HAPT chứa **42 mẫu acc + 41 mẫu gyro bị cắt
ngọn thật**. Nghĩa là HAPT **không cho biết đỉnh thật** của chuyển động người.

Phiên bản trước ghi *"HAPT lên tới 629 °/s nên phải ±1000"*. Con số 629 là do nhiễu σ=0,30 cộng
lên trên giá trị đã bão hoà — lý lẽ sai. Lý lẽ đúng:

| | lý do |
|---|---|
| **acc ±4 g** | **±2 g dùng không được**: ngưỡng va đập của bộ phát hiện té ngã là **2,0 g**, cảm biến bão hoà ở 2 g thì không bao giờ báo được "> 2 g" → nhánh sốc chết hẳn. Té ngã ở thắt lưng thường 3–6 g |
| **gyro ±1000 °/s** | ngay trên dữ liệu **đã bị cắt ở 573 °/s** mà ±500 vẫn cắt 88 mẫu; đỉnh thật còn cao hơn |
| **không cần thang nhỏ hơn** | 16 bit cho 0,00012 g/LSB và 0,031 °/s/LSB, trong khi sàn nhiễu ICM-20948 là ~0,0016 g và ~0,1 °/s ở 50 Hz — **nhiễu lớn hơn bước lượng tử hơn 10 lần**. Hạ thang đo không đo thêm được gì, chỉ mất khoảng dự trữ |

### Hệ quả: phải kẹp tín hiệu cho kênh phân loại

Model train trên dữ liệu **cắt ở 2 g / 10 rad/s**, còn board ở ±4 g / ±1000 dps sẽ báo giá trị
vượt hai mốc đó. Đặc trưng `max`, `min`, `energy` lúc chuyển động mạnh sẽ ra ngoài vùng model
từng thấy. Xử lý:

- **kênh đặc trưng phân loại**: kẹp `acc` vào ±2,0 g và `gyro` vào ±10 rad/s
- **kênh phát hiện sốc**: dùng giá trị **không kẹp**, vì nó cần biết > 2 g

Hai kênh đã tách sẵn trong thiết kế nên chỗ này khớp tự nhiên.

Bus I2C hay SPI **chưa chốt** → `icm20948.c` phải tách lớp `read_reg` / `write_reg`.

---

## 9. Điều CHƯA chứng minh được

**Không có mẫu té ngã thật nào.** Mọi số liệu ở trên chỉ chứng minh **phía âm** — không báo
động sai. Phía dương, "có té ngã mà không báo", **không đo được**.

Thử nghiệm nén thời gian các đoạn `*_TO_LIE` thật để mô phỏng chuyển tư thế nhanh như té ngã
**không dùng được làm tỷ lệ phát hiện**. Nén bằng nội suy tuyến tính **làm giảm** biên độ đỉnh:

| hệ số nén | thời gian | `a_max` lớn nhất | số ca kích hoạt được điều kiện sốc |
|---|---|---|---|
| 1 | 4,53 s | 2,412 g | 2/118 |
| 2 | 2,27 s | 1,974 g | **0/118** |
| 3–6 | 1,51–0,76 s | 1,914 g | **0/118** |

Ngưỡng `impact_g = 2,0 g` **chưa bao giờ** bị vượt ở hệ số ≥ 2. Té ngã thật có lực đập 3–10 g;
"té ngã" tổng hợp này đỉnh 1,91 g, nhẹ hơn cả đi xuống cầu thang (2,43 g). Phép thử đó chạy với
**nhánh sốc bị tắt sẵn về mặt cấu trúc**, nên nó chỉ đo được nhánh chuỗi nhãn.

Điều duy nhất nó chứng minh: chuỗi nhãn một mình không phân biệt được nằm nhanh với nằm chậm.
Điều đó vốn đã biết và chính là lý do có nhánh sốc.

Ghi lại ở đây để không ai dùng nhầm bảng số này làm tỷ lệ phát hiện té ngã.

`../DSP/test/my_samples/chuoi_nga.gt.txt` — dù tên là "chuỗi ngã" — có ground truth ghi
`23.00 24.00 STAND_TO_LIE`, tức nằm xuống **chủ động**. Theo đúng logic `har/fall.py` thì
`voluntary_guard` kích hoạt và state machine cố ý **không** báo động. Nên nó là **negative test
case**, không phải positive.

### Cách duy nhất để đóng lỗ này

- **thu mẫu thật** (đệm, có người giám sát), hoặc
- **cross-dataset**: dùng một bộ dữ liệu té ngã ngoài **chỉ làm tập test**, không tham gia
  huấn luyện. Dùng kiểu đó không vi phạm quyết định mục 1.

Ngoài ra, ba rủi ro sau **không có split nào của HAPT phủ được**, và cross-validation cũng
không bắt được vì nó chỉ so trong cùng một bộ dữ liệu:

1. **hướng gắn** — board gắn khác điện thoại trong túi thắt lưng
2. **bão hoà** — HAPT cắt ở 2 g / 573 °/s, board thì không (mục 8)
3. **sàn nhiễu** — train 0,022 g, ICM-20948 ~0,0016 g, vẫn lệch ~14 lần

Cách phát hiện: ghi ~10 phút dữ liệu thật từ board, cho qua đúng pipeline đặc trưng, rồi **so
histogram từng đặc trưng với histogram của tập train**. Rẻ, không cần nhãn, và bắt được đúng
loại lỗi mà accuracy không bao giờ chỉ ra.

---
