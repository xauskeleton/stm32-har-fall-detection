# Quyết định kỹ thuật — số liệu chứng minh

Cập nhật: 2026-09-21. Mọi bảng dưới đây sinh từ `python scripts/train.py --sweep`
(dữ liệu thô: `models/sweep.json`). Test set = 9 subject chưa từng xuất hiện trong train,
3.162 cửa sổ. Seed 42.

---

## 1. Bỏ Butterworth lowpass 20 Hz

Bảng ablation của repo DSP gốc (`../DSP/main.ipynb`, cell cuối):

| Cấu hình | #Feat | RF | SVM | MLP |
|---|---|---|---|---|
| Không Butter, không FFT | 72 | 84.25% | 84.03% | 84.31% |
| Không Butter, có FFT | 120 | 85.07% | 85.77% | 86.18% |
| Có Butter, không FFT | 72 | 84.35% | 84.66% | **83.81%** |
| Có Butter, có FFT | 120 | 85.71% | 86.37% | 86.43% |

Butterworth 20 Hz đóng góp ~+0.5%. Ô in đậm: với MLP không FFT, **bật filter còn tệ hơn tắt**.
FFT mới là thứ có ý nghĩa (+2%).

**→ Bỏ. Tiết kiệm 1 cascade biquad × 6 trục trong firmware, đổi lấy ~0.5%.**

Hệ số `FC_NOISE = 20.0` vẫn giữ trong `har/config.py` để chạy lại ablation cho báo cáo.

---

## 2. Giữ tách gravity 0.3 Hz (dù nó cũng là Butterworth)

Đây là điểm dễ hiểu nhầm: bỏ "filter" không có nghĩa bỏ luôn cái này. Lowpass 20 Hz là
**khử nhiễu**; lowpass 0.3 Hz là **tách tín hiệu** — nó định nghĩa ra `body_acc = acc − gravity`,
tức là nhóm đặc trưng thứ ba.

Bỏ `body_acc` thì pipeline không còn bộ lọc nào (hấp dẫn: firmware khỏi cần biquad,
và vấn đề lệch `filtfilt`/`sosfilt` biến mất). Nhưng giá phải trả:

| Cấu hình | #Feat | Cần IIR | logreg | Ghi chú |
|---|---|---|---|---|
| `min48` | 48 | không | 85.07% | chỉ time domain |
| `min80` | 80 | không | 86.02% | không filter nào cả |
| `body72` | 72 | có | 85.80% | |
| **`body120`** | **120** | **có** | **86.88%** | **đã chọn** (C=0.1) |
| `dsp_full` | 120 | có | 86.50% | pipeline DSP gốc, zerophase |

Chênh lệch tổng thể `min80` → `body120` là **+0.92%**. Nhìn theo lớp (cả hai cùng C=0.1,
f1-score) thì lợi ích tập trung vào đúng nhóm chuyển tư thế liên quan đến nằm:

| Lớp | f1 `min80` | f1 `body120` | Δ |
|---|---|---|---|
| `LIE_TO_STAND` | 0.54 | **0.67** | **+0.13** |
| `LIE_TO_SIT` | 0.61 | **0.69** | **+0.09** |
| `SIT_TO_LIE` | 0.63 | **0.70** | **+0.07** |
| `STAND_TO_LIE` | 0.67 | 0.66 | −0.01 |
| 6 lớp còn lại | — | — | 0.00 … +0.02 |

State machine phân biệt "nằm chủ động" với "té ngã" bằng nhãn `*_TO_LIE`, và gỡ báo động bằng
`LIE_TO_*`. `SIT_TO_LIE` tăng +0.07 và cả hai nhãn "ngồi/đứng dậy từ tư thế nằm" tăng mạnh
(+0.09, +0.13) — tức ít bị kẹt ở trạng thái `FALLEN` hơn.

Lưu ý trung thực: `STAND_TO_LIE` **không** hưởng lợi (−0.01), dù đây cũng là một nhãn mà
`voluntary_guard` phụ thuộc. Lợi ích của `body_acc` không đồng đều như kỳ vọng ban đầu.

**→ Giữ. Chi phí firmware: 1 cascade biquad bậc 3 (2 section) trên 3 trục acc, ~30 MAC/mẫu.**

Nếu sau này cần cắt tối đa: `python scripts/train.py --groups min` cho ra cấu hình 80 đặc trưng
không filter, mất ~0.9%.

---

## 3. LogisticRegression — chỉ ML cổ điển, không mạng nơ-ron

Yêu cầu đề tài: **chỉ dùng ML cổ điển**, không MLP / mạng nơ-ron. Zoo so sánh gồm
4 model tuyến tính + Naive Bayes + cây + rừng + SVM RBF:

```
Cau hinh   #Feat  IIR  FFT      logreg     linsvc        lda      ridge        gnb       tree       rf50    svm_rbf
min48         48    -    -      84.19%     84.60%     82.19%     79.25%     80.61%     80.14%     83.52%     85.04%
min80         80    -   co      85.96%     86.31%     84.60%     82.35%     77.29%     80.30%     83.78%     85.64%
body72        72   co    -      84.69%     85.48%     82.70%     80.46%     80.17%     81.37%     84.28%     84.79%
body120      120   co   co      86.88%     87.13%     85.58%     83.55%     77.23%     81.25%     84.95%     86.31%
dsp_full     120   co   co      86.43%     86.91%     85.74%     83.62%     76.60%     81.69%     84.57%     86.37%
```

Hai nhận xét:

1. **Model tuyến tính đủ tốt.** `logreg` / `linsvc` vượt cả `rf50` (84.95%) lẫn `svm_rbf`
   (86.31%) trên `body120`. Sau `StandardScaler`, 120 đặc trưng thống kê này gần như tách
   tuyến tính được — bài toán đã bão hòa, thêm độ phức tạp chỉ tổ overfit 7.767 mẫu train.
2. **`linsvc` có accuracy cao nhất nhưng KHÔNG được chọn.** Lý do ở mục dưới.

### Vì sao chọn `logreg` dù `linsvc` accuracy cao hơn

Accuracy có trọng số bị chi phối bởi 6 lớp cơ bản (mỗi lớp 420–556 mẫu test). Sáu lớp
chuyển tư thế chỉ có 10–49 mẫu nên gần như không ảnh hưởng con số tổng — nhưng chúng lại
chính là thứ bộ phát hiện té ngã dựa vào.

| Model | accuracy | macro-F1 | F1 6 lớp chuyển | F1 4 nhãn liên quan té ngã |
|---|---|---|---|---|
| **logreg C=0.1** | 86.88% | **80.69** | **73.62** | **68.10** |
| logreg C=1.0 | 86.69% | 78.29 | 68.82 | 62.79 |
| linsvc C=0.1 | 87.13% | 79.76 | 71.28 | 64.63 |
| linsvc C=0.3 | **87.25%** | 78.94 | 69.44 | 63.00 |
| lda | 85.58% | 77.72 | 68.69 | 65.63 |

`linsvc C=0.3` thắng accuracy nhưng thua cả 3 chỉ số còn lại. Nhìn theo lớp:
`STAND_TO_SIT` −0.16, `LIE_TO_SIT` −0.14, `LIE_TO_STAND` −0.05 f1 so với `logreg`.

**→ Chọn `logreg C=0.1`.** Cùng kích thước, cùng chi phí tính toán, nhưng nhận dạng chuyển
tư thế tốt hơn hẳn. Thêm nữa `logreg` cho ra xác suất (`predict_proba`) — hữu ích để đặt
ngưỡng tin cậy trên MCU; `LinearSVC` chỉ cho điểm số thô.

### Độ ổn định

Qua 3 seed (0, 1, 42) cả `logreg` lẫn `linsvc` đều có **std = 0.00** — đều là bài toán lồi,
nghiệm duy nhất, không phụ thuộc khởi tạo. Chênh lệch giữa chúng là thật, không phải nhiễu.

### Chọn C

| C | 0.01 | 0.1 | 0.3 | 1.0 | 3.0 | 10.0 |
|---|---|---|---|---|---|---|
| accuracy | 85.48% | **86.88%** | 86.72% | 86.69% | 86.65% | 86.91% |

Accuracy plateau từ C=0.1 trở lên, nhưng macro-F1 thì **giảm** khi C tăng (80.69 ở C=0.1
xuống 78.29 ở C=1.0). Chọn **C=0.1**. Lý do phụ: model train trên gia tốc kế điện thoại
nhưng sẽ chạy trên ICM-20948 đeo ở vị trí khác — regularize mạnh hơn chịu domain shift tốt hơn.

### Chi phí trên MCU

| | tham số | flash (f32) | MAC/cửa sổ |
|---|---|---|---|
| **logreg 120→12** | **1.452** | **5.7 KB** | **1.440** |
| rf50 (50 cây, depth 12) | 147.792 | 577 KB | duyệt cây |
| SVM RBF (repo gốc) | 2636 SV × 120 | **1.207 KB** | ~316.000 |

Kiểm chứng SVM: `support_vectors_.shape = (2636, 120)` — không vừa flash 512 KB, đúng như
ghi chú ban đầu. `rf50` cũng không vừa.

Inference trên MCU = 1 phép nhân ma trận 120×12 + argmax. Softmax chỉ cần khi muốn lấy
độ tin cậy.

---

## 4. Lọc causal, không dùng `filtfilt`

`filtfilt` chạy forward-backward nên cần biết cả tín hiệu tương lai → không real-time được.
Lo ngại ban đầu là train (zerophase) / deploy (causal) lệch nhau sẽ mất accuracy. Đo thực tế:

| mode | logreg |
|---|---|
| `causal` (`sosfilt`) | **86.88%** |
| `zerophase` (`sosfiltfilt`) | 86.50% |

Causal **không hề kém hơn** — đây là lựa chọn miễn phí, không có đánh đổi nào.

---

## 5. Phát hiện té ngã từ nhãn đầu ra, không thêm lớp thứ 13

Giữ nguyên quyết định cũ. HAPT không có nhãn FALL, nhưng nằm xuống chủ động luôn đi qua
`STAND_TO_LIE`/`SIT_TO_LIE`, còn té ngã thì nhảy thẳng từ `WALKING`/`STANDING` sang `LAYING`.

**Vấn đề còn treo:** hiện **không có mẫu té ngã thật nào**. File `chuoi_nga.gt.txt` ở
`../DSP/test/my_samples/` — dù tên là "chuỗi ngã" — có ground truth ghi rõ
`23.00 24.00 STAND_TO_LIE`, tức là nằm xuống **chủ động**. Theo đúng logic `har/fall.py`
thì `voluntary_guard` sẽ kích hoạt và state machine cố ý **không** báo động.
Nên nó là **negative test case**, không phải positive. Xem `docs/TODO.md` mục câu hỏi treo.

---

## 6. Phần cứng

STM32F446RET6 (Cortex-M4F 180 MHz, 512 KB flash, 128 KB SRAM) + ICM-20948.

Chỉ dùng **accelerometer + gyroscope**. Bỏ magnetometer dù ICM-20948 có: HAPT được thu bằng
Samsung Galaxy S II chỉ với acc + gyro, thêm từ kế vào lúc inference sẽ tạo ra đặc trưng mà
model chưa từng thấy khi train.

Ngân sách tài nguyên còn rất rộng so với nhu cầu:

| | cần | có |
|---|---|---|
| flash (model) | 5.7 KB | 512 KB |
| RAM (ring buffer 128×6 float + scratch) | ~10 KB | 128 KB |
| tính toán | 12 FFT 128 điểm + 1.440 MAC, mỗi 1.28 s | 180 MHz M4F |

Bus I2C hay SPI **chưa chốt** → `icm20948.c` phải tách lớp `read_reg`/`write_reg`.

---

## 7. Bỏ ngưỡng sốc gia tốc — chỉ dùng chuỗi nhãn

Đo bằng `python scripts/eval_fall.py`, chạy state machine trên **toàn bộ 61 experiment HAPT
liên tục (372,3 phút, 17.453 cửa sổ)**. HAPT không có té ngã, nên **mọi** sự kiện FALL sinh ra
đều là báo nhầm. Chạy hai lần để tách nguyên nhân: `oracle` nạp nhãn thật từ `labels.txt`,
`model` nạp nhãn do bộ phân loại sinh ra.

| `shock_rule` | báo nhầm (oracle) | báo nhầm (model) | tần suất (model) |
|---|---|---|---|
| **`labels`** | **0** | **1** | 1 lần / 6,2 giờ |
| `both` | 77 | 87 | 1 lần / 4,3 phút |
| `any` | 120 | 138 | 1 lần / 2,7 phút |

Hai kết luận:

1. **Logic state machine đúng.** Với nhãn hoàn hảo và `shock_rule='labels'`: **0 báo nhầm
   trên 118 lần nằm xuống chủ động**. `voluntary_guard` hoạt động như thiết kế.
2. **Ngưỡng sốc gia tốc là thứ gây ra toàn bộ 77–120 báo nhầm kia**, không phải bộ phân loại.

### Vì sao ngưỡng sốc không dùng được trên HAPT

Chuỗi nhãn ngay trước mỗi lần báo nhầm là `LAYING LAYING LAYING LAYING` — báo nhầm xảy ra
**giữa lúc đang nằm yên**, không phải lúc chuyển tư thế. Đo trên đoạn LAYING dài nhất của
HAPT (33,9 s):

| | giá trị |
|---|---|
| std mỗi trục khi nằm yên | **0,30 g** |
| thay đổi của `abs(a)` giữa 2 mẫu liên tiếp | trung bình **0,32 g**, đỉnh **1,49 g** |
| `abs(a)` trên cả đoạn | tb 1,10 g, min 0,24 g, max 2,16 g |

Cơ thể người đang nằm không thể thay đổi 1,5 g trong 20 ms — đó là **nhiễu cảm biến thuần**.
Gia tốc kế của Galaxy S II (2011) có noise floor rất cao.

Vì `freefall_g` / `impact_g` đo trên **min/max từng mẫu** trong cửa sổ, chúng bị nhiễu chi phối
hoàn toàn:

| nhãn | a_min ≤ 0,6g | a_max ≥ 2,0g | cả hai |
|---|---|---|---|
| LAYING (thô) | 99,7% | 12,7% | **12,6%** |
| LAYING (lọc 20 Hz) | 97,4% | 3,8% | **3,6%** |
| STANDING (lọc 20 Hz) | 95,1% | 4,1% | 3,8% |

Lọc lowpass 20 Hz giảm được 12,6% → 3,6% nhưng **không cứu được** — vẫn 97% cửa sổ nằm yên
bị coi là "đang rơi tự do". Đây cũng là điểm sửa lại nhận định ở mục 1: Butterworth 20 Hz
vô dụng *cho bộ phân loại*, nhưng nó *có* tác dụng cho phát hiện sốc — chỉ là không đủ.

**→ Đặt `shock_rule='labels'` làm mặc định.** Hai ngưỡng vẫn giữ trong `FallParams` như một
điểm móc, **phải đo lại và tune trên dữ liệu ICM-20948 thật** (noise floor thấp hơn nhiều)
trước khi bật lại `'both'`.

### Điều này CHƯA chứng minh được gì

Con số 1 báo nhầm / 6,2 giờ chỉ là **phía âm**. Phía dương — "có té ngã mà không báo" —
**không đo được** vì không có mẫu té ngã (xem `docs/TODO.md` câu hỏi 1).

Rủi ro cụ thể: với `shock_rule='labels'`, điều kiện báo động là nhãn nhảy thẳng từ
tư thế đứng/ngồi sang `LAYING` mà **không** qua `*_TO_LIE`. Nếu khi té ngã thật bộ phân loại
lại xuất ra `STAND_TO_LIE` (vì tín hiệu có phần giống), `voluntary_guard` sẽ chặn báo động.
Té ngã chưa từng có trong dữ liệu train nên **không thể biết trước** điều này, phải đo thật.

---

## 8. Lọc median 3 cho kênh phát hiện sốc

Mục 7 kết luận phải bỏ ngưỡng sốc vì nhiễu. Kết luận đó **thiếu một bước tiền xử lý**.

Pipeline chuẩn của UCI HAR là **median 3 + Butterworth 20 Hz**. Butterworth đã bị bỏ (mục 1,
hợp lý cho bộ phân loại) nhưng bước median thì chưa từng có trong repo này. Nó chính là bước
xử lý gai nhiễu đơn lẻ — đúng thứ phá ngưỡng sốc.

Tỷ lệ cửa sổ thỏa điều kiện sốc (`a_min ≤ 0,6g` VÀ `a_max ≥ 2,0g`):

| Làm mượt `abs(a)` | Tư thế tĩnh (phải ≈ 0%) | Đi xuống cầu thang (lực đập thật) |
|---|---|---|
| không | 12,9% ✗ | 97,7% |
| lowpass 20 Hz | 3,4% ✗ | 95,6% |
| **median 3** | **0,0%** ✓ | **74,9%** ✓ |
| median 3 + lp 10 Hz | 0,0% | 65,8% |
| median 3 + lp 3 Hz | 0,9% | 0,9% ✗ quá mượt |

**→ `median 3` là điểm ngọt.** Xóa sạch báo sốc giả ở tư thế tĩnh mà vẫn giữ 74,9% độ nhạy
với lực đập thật. Chi phí trên MCU: 3 phép so sánh mỗi trục mỗi mẫu, ~10 phép tính/mẫu.

Áp dụng: `har/config.py` thêm `MEDIAN_SHOCK = 3`, `har/dsp.py` thêm `median3()`,
`har/fall.py` thêm `shock_windows()`. **Chỉ dùng cho kênh sốc, KHÔNG dùng cho đặc trưng
phân loại** (đặc trưng vẫn tính trên tín hiệu thô, như mục 1–3).

`shock_rule` trở về mặc định `'both'`. Báo nhầm sau khi sửa, trên 372 phút HAPT:

| | báo nhầm |
|---|---|
| nhãn hoàn hảo | **0** |
| nhãn từ model | **2** (1 lần / 3,1 giờ) |

---

## 9. Giới hạn của thử nghiệm pseudo-fall

`scripts/pseudo_fall.py` nén thời gian các đoạn `*_TO_LIE` thật để mô phỏng chuyển tư thế
nhanh như té ngã. Kết quả: ở 0,76 s, bộ phân loại **vẫn** gán nhãn `*_TO_LIE` trong 97,5%
trường hợp, tỷ lệ báo động 0,8%.

Thoạt nhìn kết quả này gợi ý rằng giả định của mục 5 ("té ngã thì không qua nhãn transition")
là sai và chuỗi nhãn không dùng được. **Suy luận đó không có cơ sở.**

Lý do: nén thời gian bằng nội suy tuyến tính **làm giảm** biên độ đỉnh, không tăng. Đo lại:

| k | thời gian | `a_max` lớn nhất | số ca kích hoạt được điều kiện sốc |
|---|---|---|---|
| 1 | 4,53 s | 2,412 g | 2/118 |
| 2 | 2,27 s | 1,974 g | **0/118** |
| 3–6 | 1,51–0,76 s | 1,914 g | **0/118** |

Ngưỡng `impact_g = 2,0 g` **chưa bao giờ** bị vượt ở k ≥ 2. Té ngã thật có lực đập 3–10 g;
"té ngã" tổng hợp này đỉnh 1,91 g — nhẹ hơn cả đi xuống cầu thang (2,43 g). Nghĩa là phép thử
đó chạy với **nhánh sốc bị tắt sẵn về mặt cấu trúc**, nên nó chỉ đo được nhánh chuỗi nhãn.

**Điều duy nhất nó chứng minh:** chuỗi nhãn một mình không phân biệt được nằm nhanh với nằm
chậm. Điều đó vốn đã biết và chính là lý do có nhánh sốc.

**Điều nó KHÔNG chứng minh:** tỷ lệ bỏ sót té ngã thật. Vẫn phải đo bằng mẫu thật.

Script in thêm cột `a_max max` và in rõ giới hạn này ngay trong kết quả, để không ai
dùng nhầm bảng số này làm tỷ lệ phát hiện té ngã.

---

## 10. float32, không quantize int8

Câu hỏi: firmware có cần đưa model về int8 không?

**Không.** STM32F446RET6 là Cortex-M4F — có **FPU cứng single-precision**, phép `float` chạy
1 chu kỳ. int8 chỉ có lợi khi MCU không có FPU (M0/M3) hoặc model lớn cỡ MB. Ở đây cả hai
điều kiện đều không có: model 5,7 KB / 512 KB flash, tính toán 0,034% CPU.

Đo trên 3.530 cửa sổ (12 experiment), so với `float64` của Python:

| Cách lưu trọng số | Kích thước W | Sai số trọng số | Số cửa sổ lệch nhãn |
|---|---|---|---|
| **float32** | **5,6 KB** | 0 | **0 / 3.530 (0,000%)** |
| int16 | 2,8 KB | 2,6e-05 | 0 / 3.530 (0,000%) |
| int8 | 1,4 KB | 6,4e-03 | 13 / 3.530 (**0,368%**) |

int8 tiết kiệm 4,2 KB (trên 512 KB flash — tức 0,8%) nhưng đổi lấy 0,368% cửa sổ sai nhãn.
Không đáng. int16 miễn phí về độ chính xác nhưng tiết kiệm 2,8 KB cũng không giải quyết vấn đề gì.

### Điều CẦN chú ý thay vì int8

**Toàn bộ firmware phải dùng `float`, tuyệt đối không dùng `double`.** FPU của Cortex-M4
không có double precision — compiler sẽ emulate bằng software, chậm 50–100 lần. Một biến
`double` lọt vào vòng lặp tính đặc trưng là đủ phá toàn bộ ngân sách thời gian.

Cạm bẫy thường gặp trong C: `1e-10` và hằng số thập phân không có hậu tố là `double`.
Phải viết `1e-10f`, `0.5f`, và dùng `sqrtf()` / `fabsf()` chứ không phải `sqrt()` / `fabs()`.

### Sai số float32 vs float64 — dùng cho test đối chiếu

| Nhóm đặc trưng | Sai số tuyệt đối TB | Lớn nhất |
|---|---|---|
| mean, std, max, min, rms, mad | 1,6e-07 … 2,4e-06 | 4,9e-05 |
| fft_max | 3,4e-05 | 3,8e-03 |
| fft_mean | 4,2e-06 | 7,8e-05 |
| fft_energy | 6,3e-05 | **6,4e-03** |
| mean_freq | 1,4e-05 | 3,2e-04 |
| **tất cả** | **1,2e-05** | **6,4e-03** |

**`firmware/test/test_parity.c` phải dùng ngưỡng TUYỆT ĐỐI (1e-3), không dùng tương đối.**
Lý do: `body_acc_*_mean` vốn ≈ 0 theo định nghĩa (`body_acc = acc − lowpass(acc)`), nên sai số
tương đối ở đó lên tới **1,96** dù sai số tuyệt đối chỉ 1,7e-05. Tiêu chí tương đối thuần
sẽ fail oan ở 6/120 đặc trưng.
