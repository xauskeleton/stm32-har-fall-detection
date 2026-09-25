# Nhận dạng hành động người và phát hiện té ngã trên STM32

Phân loại 12 hành động từ tín hiệu gia tốc kế + con quay hồi chuyển, và phát hiện té ngã,
chạy **hoàn toàn trên vi điều khiển** — không cần kết nối máy tính hay đám mây.

Phần cứng đích: **STM32F446RET6** (Cortex-M4F 180 MHz, 512 KB flash, 128 KB SRAM) +
**ICM-20948** (chỉ dùng accelerometer + gyroscope).

## Kết quả

Huấn luyện trên [HAPT Dataset](https://archive.ics.uci.edu/dataset/341/) (Đại học Genoa),
chia train/test **theo người** — 21 người để train, 9 người hoàn toàn chưa từng thấy để test.

| | |
|---|---|
| Accuracy 12 lớp | **93,23%** |
| macro-F1 | **85,81** |
| F1 4 nhãn chuyển tư thế liên quan đến nằm | **69,16** |
| Model | LogisticRegression 160 → 12 |
| Đặc trưng | 516 tính ra, **RFE giữ 160** |
| Số tham số | **1.932** (7,5 KB float32) |
| Flash thật đã dùng | **161,7 KB / 512 KB** (31,6%) |
| RAM thật đã dùng | **~15,2 KB / 128 KB** (11,9%) |

Chia train/test theo **người**, dùng đúng split chính thức của HAPT. Để so các phương án mà
không chạm vào tập test, dùng cross-validation 7 fold trong tập train: **94,69% ± 1,92**,
macro-F1 **87,73 ± 2,84**, F1 4 nhãn ngã **75,32 ± 7,01**.

Firmware C đã có đủ trong `firmware/Core/` và đối chiếu với Python trên toàn bộ tập test:
**0 / 3.156 cửa sổ lệch nhãn**, accuracy giống hệt. Chạy kiểm tra: `make -C firmware test`.

Nhìn theo mức độ mà bộ phát hiện té ngã thực sự dùng (gộp "đi/đứng/ngồi" thành một nhóm):

| Nhóm | Số mẫu | Recall | Precision |
|---|---|---|---|
| ACTIVE (đi, đứng, ngồi) | 2.451 | 99,9% | 99,5% |
| **LAYING** | 545 | **99,1%** | **99,8%** |
| Ngồi/đứng dậy (`STAND_TO_SIT`, `SIT_TO_STAND`) | 33 | 87,9% | 93,5% |
| Nằm xuống (`*_TO_LIE`) | 81 | 92,6% | 93,8% |
| Đứng dậy (`LIE_TO_*`) | 52 | 92,3% | 98,0% |
| **Tổng (đúng nhóm)** | **3.162** | **99,30%** | |

Báo động té ngã sai, đo trên **toàn bộ 372 phút HAPT** (dữ liệu không chứa té ngã, nên mọi
báo động đều là báo sai), gồm 118 lần nằm xuống chủ động:

| | Số lần báo sai |
|---|---|
| Nếu nhãn hoàn hảo | **0** |
| Với nhãn thật từ model | **0** |

> **Giới hạn cần biết:** chưa có mẫu té ngã thật nào để đo tỷ lệ **phát hiện đúng**.
> Toàn bộ số liệu trên chỉ nói về việc *không báo động sai*. Xem `docs/TODO.md`.

## Pipeline

```
ICM-20948 @ 50 Hz  →  ax ay az gx gy gz (đơn vị g và rad/s)
        │
        ├─────────────────────────────┐
        ▼                             ▼
 Tách gravity                   Lọc median 3 mẫu
 Butterworth bậc 3, 0,3 Hz      (xoá gai nhiễu đơn lẻ)
 causal — chạy real-time                │
 body_acc = acc − gravity               ▼
        │                        |a| min/max mỗi cửa sổ
        ▼                        rơi tự do? đập sàn?
 Cửa sổ 128 mẫu (2,56 s)                │
 trượt 64 mẫu (overlap 50%)             │
        │                               │
        ▼                               │
 516 đặc trưng                          │
  5 nhóm tín hiệu: body_acc, gravity,   │
    body_acc_jerk, gyro, gyro_jerk      │
  + magnitude + phổ FFT (bỏ bin DC)     │
  × mean std mad max min energy iqr     │
    entropy arCoeff(Burg-4) correlation │
    sma skewness kurtosis bandsEnergy   │
    maxInds meanFreq + 7 góc gravity    │
        │                               │
        ▼                               │
 RFE giữ 160 cột (models/selected.json) │
        │                               │
        ▼                               │
 Chuẩn hoá Z-score (StandardScaler)     │
        │                               │
        ▼                               │
 LogisticRegression 160 × 12 + argmax   │
        │                               │
        ▼                               ▼
    nhãn 1..12  ────────────►  State machine phát hiện té ngã
                               IDLE → SHOCK → FALLEN
```

State machine té ngã chỉ là if/else trên chuỗi nhãn, cộng 2 ngưỡng so sánh trên `|a|`:

```
IDLE   ── có sốc gia tốc HOẶC nhãn nhảy thẳng sang LAYING ──►  SHOCK
SHOCK  ── LAYING 2 cửa sổ liên tiếp ──►  FALLEN   (BÁO ĐỘNG)
SHOCK  ── thấy nhãn SIT_TO_LIE / STAND_TO_LIE ──►  IDLE   (nằm chủ động, bỏ qua)
SHOCK  ── quay lại đứng/đi ──►  IDLE                      (vấp nhẹ, tự dậy)
FALLEN ── rời LAYING 2 cửa sổ ──►  IDLE                   (đã dậy)
```

Ý tưởng: HAPT không có nhãn té ngã, nhưng 12 nhãn của nó đủ để phân biệt. Nằm xuống chủ động
luôn đi qua `STAND_TO_LIE` / `SIT_TO_LIE`; té ngã thì nhảy thẳng từ `WALKING`/`STANDING` sang
`LAYING`, kèm một cú sốc gia tốc.

## 12 hành động

6 cơ bản: `WALKING` `WALKING_UPSTAIRS` `WALKING_DOWNSTAIRS` `SITTING` `STANDING` `LAYING`

6 chuyển tư thế: `STAND_TO_SIT` `SIT_TO_STAND` `SIT_TO_LIE` `LIE_TO_SIT` `STAND_TO_LIE` `LIE_TO_STAND`

## Cấu trúc

```
har/                    thư viện Python dùng chung
  config.py             hằng số DSP, 12 nhãn, chia train/test theo người
  dsp.py                median 3, lowpass 20 Hz, tách gravity, jerk, 516 đặc trưng
  dataset.py            đọc HAPT RawData, Phyphox CSV, log UART, ground truth
  fall.py               state machine té ngã (bản tham chiếu của fall_detect.c)
  model.py              chọn cột + scaler + clf, đánh giá, lưu/nạp bundle
  export_c.py           sinh code C từ model đã huấn luyện
scripts/
  train.py              RFE (chỉ trên người train) + huấn luyện, lưu models/
  eval_fall.py          đo báo động té ngã sai trên toàn bộ HAPT
  export_model.py       sinh firmware/.../model_params.*
models/                 selected.json (160 cột), scaler.pkl, classifier.pkl, meta.json
firmware/               sinh tự động — KHÔNG sửa tay
  Core/Inc/model_params.h    hằng số, ngưỡng, khai báo extern
  Core/Src/model_params.c    biquad, ZI, HAR_BANDS, HAR_SELECTED, scaler, ma trận 12×160
  Core/Src/har_dsp.c         median 3, cascade biquad, FFT 128, 14 phép thống kê   (viết tay)
  Core/Src/har_features.c    516 đặc trưng                                         (viết tay)
  Core/Src/har_classifier.c  chọn cột + chuẩn hoá + nhân ma trận + argmax          (viết tay)
  Core/Src/har_pipeline.c    ring buffer, chạy mỗi 64 mẫu mới                      (viết tay)
  Core/Src/fall_detect.c     bản sao của har/fall.py                               (viết tay)
  Core/Src/har_app.c         hoán vị trục + kẹp tín hiệu, không phụ thuộc HAL      (viết tay)
  test/test_parity.c         đối chiếu C vs Python, make -C firmware test          (viết tay)
docs/
  DECISIONS.md          số liệu chứng minh từng quyết định kỹ thuật
  HARDWARE.md           phần cứng thực tế: clock, cảm biến, flash/RAM đo từ build
  TODO.md               việc còn lại và câu hỏi còn treo
```

## Cài đặt

```bash
pip install -r requirements.txt
```

Model đã huấn luyện sẵn nằm trong `models/`, dùng được ngay không cần train lại.

## Dữ liệu

Repo **không** chứa HAPT dataset. Chỉ cần nếu muốn huấn luyện lại — tải thư mục `RawData/`
từ [bản gốc trên UCI](https://archive.ics.uci.edu/dataset/341/):

```bash
export HAPT_RAWDATA=/đường/dẫn/tới/RawData
# hoặc
python scripts/train.py --data /đường/dẫn/tới/RawData
```

`har.dataset.find_rawdata()` tự dò các vị trí quen thuộc.

**Phải dùng đúng bản gốc.** Có những bản HAPT lưu hành đã bị cộng nhiễu để làm bài tập khử
nhiễu; huấn luyện trên bản đó mất ~6% accuracy mà không báo lỗi gì. Dấu hiệu: giá trị ghi 16
chữ số liên tục (dữ liệu thật bị lượng tử hoá 1/720 g) và std dư khi đứng yên ~0,29 g thay vì
~0,022 g. Xem `docs/DECISIONS.md` mục 0.

Nếu đổi dữ liệu thì **xoá `data_raw/hapt_raw.npz`** — cache giữ tín hiệu cũ.

## Lệnh hay dùng

```bash
# Huấn luyện lại
python scripts/train.py

# Đo báo động té ngã sai trên toàn bộ HAPT
python scripts/eval_fall.py

# Sinh code C cho firmware
python scripts/export_model.py

# Thêm --cache để lần sau đọc dữ liệu nhanh hơn nhiều (60 s -> 1 s)
python scripts/train.py --cache hapt_raw.npz
```

Code cố định vào đúng cấu hình đã chốt — không còn cờ bật/tắt để thí nghiệm. Số liệu của các
bảng so sánh (19 model, 11 cấu hình đặc trưng, 7 mức RFE) nằm trong `docs/DECISIONS.md`.

**Chạy lại `export_model.py` sau mỗi lần `train.py`** — nếu quên thì firmware dùng trọng số cũ
mà không báo lỗi gì.

## Các quyết định kỹ thuật chính

Mỗi quyết định đều có số liệu chứng minh trong `docs/DECISIONS.md`:

1. **Phát hiện té ngã từ nhãn đầu ra** của bộ phân loại 12 lớp, không thêm lớp thứ 13, không
   dùng dataset té ngã ngoài để huấn luyện. Nằm xuống chủ động luôn đi qua `*_TO_LIE`; té ngã
   nhảy thẳng từ `WALKING`/`STANDING` sang `LAYING`, kèm sốc gia tốc
2. **LogisticRegression, không phải mạng nơ-ron** — quét 19 cấu hình. XGBoost 200×d4 vừa flash
   (225 KB) và hơn ở F1 té ngã, nhưng **đặc trưng quan trọng hơn model**: logreg + bộ đặc trưng
   tốt hơn thắng cả XGBoost + bộ đặc trưng cũ, mà vẫn 7,5 KB và giải thích được trong một câu
3. **Tính 516 đặc trưng, RFE giữ 160** — cắt bớt làm model TỐT HƠN: dùng cả 516 cho 73,33 F1 ngã, giữ 160 cho 75,32.
   Khác biệt nằm ở *đặc trưng nào*, không phải *bao nhiêu*
4. **Bỏ Butterworth lowpass 20 Hz, giữ tách gravity 0,3 Hz** — lowpass thắng 3/7 fold, không đủ
   bằng chứng; tách gravity thì bỏ đi mất 2,26 macro-F1
5. **Lọc causal, không dùng `filtfilt`** — `filtfilt` không chạy real-time được, và đo thực tế
   causal không hề kém hơn
6. **float32, không quantize int8** — và bản C đã đối chiếu 0/3.162 cửa sổ lệch nhãn. int8 làm 0,063% cửa sổ sai nhãn
7. **So các phương án bằng cross-validation trong tập train**, không bằng một lần đọc tập test.
   Độ lệch giữa người là ±2…3% nên chỉ ≥ 6/7 fold mới coi là bằng chứng

## Trạng thái

Phần Python xong và đã đo đạc đầy đủ. Tham số model đã xuất sang C và biên dịch được.

Firmware STM32CubeIDE đã có ở branch `Embeded_System` và **đã build được**: flash
161,7 KB / 512 KB, RAM ~15,2 KB / 128 KB. Thông số phần cứng thật ở `docs/HARDWARE.md`.

Việc còn lại trước khi firmware chạy đúng: hoán vị trục cảm biến, kẹp tín hiệu cho kênh phân
loại (HAPT bão hoà ở 2 g / 573 °/s nhưng board thì không), và cài các phép còn thiếu trong C
(`arCoeff` Burg-4, `entropy` 256 ô, `bandsEnergy`) — xem `docs/TODO.md`.
