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
| Accuracy 12 lớp | **86,88%** |
| macro-F1 | **80,69** |
| Model | LogisticRegression 120 → 12 |
| Số tham số | **1.452** (5,7 KB float32) |
| Thời gian tính 1 cửa sổ | **0,44 ms** trên M4F 180 MHz → **0,034% CPU** |
| RAM cần | ~4,5 KB / 128 KB |

Nhìn theo mức độ mà bộ phát hiện té ngã thực sự dùng (gộp "đi/đứng/ngồi" thành một nhóm):

| Nhóm | Số mẫu | Recall | Precision |
|---|---|---|---|
| ACTIVE (đi, đứng, ngồi) | 2.451 | 99,9% | 99,6% |
| **LAYING** | 545 | **99,8%** | **100,0%** |
| Nằm xuống (`*_TO_LIE`) | 81 | 92,6% | 96,2% |
| Đứng dậy (`LIE_TO_*`) | 52 | 90,4% | 97,9% |
| **Tổng** | **3.162** | **99,43%** | |

Báo động té ngã sai, đo trên **toàn bộ 372 phút HAPT** (dữ liệu không chứa té ngã, nên mọi
báo động đều là báo sai), gồm 118 lần nằm xuống chủ động:

| | Số lần báo sai |
|---|---|
| Nếu nhãn hoàn hảo | **0** |
| Với nhãn thật từ model | **2** (1 lần / 3,1 giờ) |

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
 120 đặc trưng                          │
  3 nhóm: total_acc, body_acc, gyro     │
  × 4 kênh: x, y, z, magnitude          │
  × 10 đặc trưng:                       │
     thời gian (6) mean std max min RMS MAD
     tần số (4)   FFT max/mean/energy, meanFreq
        │                               │
        ▼                               │
 Chuẩn hoá Z-score (StandardScaler)     │
        │                               │
        ▼                               │
 LogisticRegression 120 × 12 + argmax   │
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
  dsp.py                Butterworth SOS, median 3, tách gravity, cắt cửa sổ, 120 đặc trưng
  dataset.py            đọc HAPT RawData, Phyphox CSV, log UART, ground truth
  fall.py               state machine té ngã (bản tham chiếu của fall_detect.c)
  model.py              đánh giá, lưu/nạp model, xuất trọng số cho C
  export_c.py           sinh code C từ model đã huấn luyện
scripts/
  train.py              huấn luyện, lưu models/
  eval_fall.py          đo báo động té ngã sai trên toàn bộ HAPT
  export_model.py       sinh firmware/.../model_params.*
models/                 scaler.pkl, classifier.pkl, meta.json, sweep.json
firmware/               sinh tự động — KHÔNG sửa tay
  Core/Inc/model_params.h    hằng số, ngưỡng, khai báo
  Core/Src/model_params.c    hệ số biquad, scaler, ma trận 12×120
  test/golden_vectors.h      dữ liệu đối chiếu C vs Python
docs/
  DECISIONS.md          10 mục — số liệu chứng minh từng quyết định kỹ thuật
  TODO.md               việc còn lại và câu hỏi còn treo
```

## Cài đặt

```bash
pip install -r requirements.txt
```

Model đã huấn luyện sẵn nằm trong `models/`, dùng được ngay không cần train lại.

## Dữ liệu

Repo **không** chứa HAPT dataset (409 MB). Chỉ cần nếu muốn huấn luyện lại:

```bash
export HAPT_RAWDATA=/đường/dẫn/tới/RawData
# hoặc
python scripts/train.py --data /đường/dẫn/tới/RawData
```

`har.dataset.find_rawdata()` tự dò các vị trí quen thuộc.

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

Các script so sánh cấu hình (5 bộ đặc trưng × 8 model, thử nén thời gian) đã được gỡ bỏ
sau khi chốt xong — số liệu còn nguyên trong `docs/DECISIONS.md` và `models/sweep.json`,
code nằm ở commit `8174182` nếu cần chạy lại.

## Các quyết định kỹ thuật chính

Mỗi quyết định đều có số liệu chứng minh trong `docs/DECISIONS.md`:

1. **Bỏ Butterworth lowpass 20 Hz** — chỉ đóng góp ~0,5% accuracy, có trường hợp còn làm tệ hơn
2. **Giữ tách gravity 0,3 Hz** — không phải khử nhiễu mà là tách tín hiệu, đem lại +0,92%
3. **LogisticRegression, không phải mạng nơ-ron** — thắng mọi model khác, kể cả SVM RBF và
   Random Forest, trong khi nhỏ hơn 200 lần
4. **Lọc causal, không dùng `filtfilt`** — `filtfilt` không chạy real-time được, và đo thực tế
   causal không hề kém hơn
5. **Phát hiện té ngã từ nhãn đầu ra**, không thêm lớp thứ 13, không dùng dataset té ngã ngoài
6. **Lọc median 3 cho kênh phát hiện sốc** — nhiễu gia tốc kế làm 12,9% cửa sổ tư thế tĩnh bị
   coi là té ngã; median 3 đưa về 0,0%
7. **float32, không quantize int8** — Cortex-M4F có FPU cứng nên int8 không nhanh hơn, mà còn
   làm 0,368% cửa sổ sai nhãn

## Trạng thái

Phần Python xong và đã đo đạc đầy đủ. Tham số model đã xuất sang C và biên dịch được.

Còn thiếu phần thuật toán C (lọc median, biquad, FFT, 120 đặc trưng, nhân ma trận,
state machine) và phần đọc cảm biến — xem `docs/TODO.md`.
