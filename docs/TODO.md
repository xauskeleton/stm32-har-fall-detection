# Tiến độ

## Đã xong

**Thư viện Python** (`har/`) — cố định vào đúng cấu hình đã chốt, không còn núm điều khiển
thí nghiệm nào:

- `config.py` — hằng số DSP, 12 nhãn, nhóm nhãn cho state machine, chia train/test theo người
- `dsp.py` — median 3, tách gravity (Butterworth SOS causal), cắt cửa sổ, 120 đặc trưng
- `dataset.py` — đọc HAPT (có cache .npz), Phyphox CSV, log UART, ground truth
- `fall.py` — state machine IDLE → SHOCK → FALLEN
- `model.py` — đánh giá, lưu/nạp bundle, `linear_weights()` cho export C
- `export_c.py` — sinh `model_params.{h,c}` + `golden_vectors.h`

**Script** (`scripts/`) — `train.py`, `eval_fall.py`, `export_model.py`

**Kết quả đã đo:**

- Model: LogisticRegression 120 → 12, C=0.1, causal — **86,88%** / macro-F1 **80,69**,
  1.452 tham số (5,7 KB)
- Gộp nhãn theo mức state machine dùng: **99,43%**, riêng `LAYING` có **0 dương tính giả**
- Báo động té ngã sai: **0** với nhãn hoàn hảo, **2 / 372 phút** với nhãn thật từ model
- Chi phí: **0,44 ms/cửa sổ** trên M4F 180 MHz = 0,034% CPU, RAM ~4,5 KB

**Tham số model đã xuất sang C** và biên dịch sạch với `gcc -Wall -Wextra -Wdouble-promotion`.

## Chưa làm — thuật toán C (làm được ngay, máy đã có gcc)

Toàn bộ phần này là C thuần, không đụng tới chip, kiểm chứng được trên PC bằng
`firmware/test/golden_vectors.h`:

- [ ] `median3.c` — lọc median cửa sổ 3 (xem `DECISIONS.md` mục 8)
- [ ] `dsp_filter.c` — cascade biquad DF2T bậc 3 cho lọc 0,3 Hz, state giữ qua các mẫu
- [ ] `fft.c` — FFT radix-2 128 điểm tự viết; để ngỏ đường dùng `arm_rfft_fast_f32`
- [ ] `features.c` — 120 đặc trưng. Cần `median_f32` (quickselect, N chẵn → trung bình
      2 phần tử giữa) để khớp `np.median`
- [ ] `classifier.c` — chuẩn hoá + nhân ma trận 120×12 + argmax
- [ ] `fall_detect.c` — bản C của `har/fall.py`, phải khớp từng nhánh
- [ ] `har_pipeline.c` — ring buffer 128 mẫu, cứ 64 mẫu mới thì chạy 1 lần
- [ ] `test/test_parity.c` + `Makefile` target `test` (gcc trên PC).
      **Dùng sai số TUYỆT ĐỐI < 1e-3, không dùng sai số tương đối.** Đã đo trên 3.530 cửa sổ:
      tuyệt đối trung bình 1,2e-05, lớn nhất 6,4e-03. Tiêu chí tương đối sẽ **fail oan** ở
      `body_acc_*_mean` — đặc trưng này vốn ≈ 0 nên sai số tương đối lên tới 1,96.

**Toàn bộ dùng `float`, KHÔNG dùng `double`** — FPU của Cortex-M4 chỉ có single precision,
`double` bị emulate bằng software, chậm 50–100 lần. Hằng số phải có hậu tố `f` (`0.5f`,
`1e-10f`), hàm phải là `sqrtf()` / `fabsf()`. Đã đo: float32 cho kết quả **giống hệt**
float64, 0/3.530 cửa sổ khác nhãn. Không cần quantize int8.

## Chưa làm — phần cứng (chờ có board)

- [ ] Chốt toolchain (khuyến nghị STM32CubeIDE — một bộ cài, có sẵn trình biên dịch + nạp)
- [ ] Chốt bus cảm biến: I2C hay SPI
- [ ] `icm20948.c` — driver, tách lớp `read_reg`/`write_reg` để đổi I2C↔SPI chỉ sửa 2 hàm.
      Cấu hình ±4g / ±500 dps, output 50 Hz.
      **Đổi đơn vị sang g và rad/s cho khớp HAPT.**
- [ ] `board.c` — timer ngắt 50 Hz, UART, LED
- [ ] `main.c` — vòng lặp chính
- [ ] Đo thực tế: thời gian 1 cửa sổ bằng `DWT->CYCCNT`, RAM/flash thật

## Chưa làm — phần Python

- [ ] `scripts/infer_file.py` — suy luận offline trên file đo tay + chạy state machine té ngã,
      in chuỗi nhãn theo thời gian và sự kiện té ngã
- [ ] Copy vài sample đo tay từ `../DSP/test/my_samples/` sang `test/my_samples/`
- [ ] Sửa `har/fall.py` nhánh chết trong `STATE_IDLE`: `_laying` tối đa bằng 1 nhưng so với
      `laying_confirm = 2` nên không bao giờ đúng. Quyết định bỏ hay sửa.

## Câu hỏi còn treo

1. **Không có mẫu té ngã thật nào** — đây là nút chặn lớn nhất.
   Mọi số liệu hiện có chỉ chứng minh *không báo động sai*, chưa chứng minh *phát hiện được*.
   Ba hướng:
   - (a) tự đo: đẩy board/điện thoại xuống đệm, viết `.gt.txt` **không có** nhãn transition
   - (b) chỉ đánh giá phía âm, nói rõ giới hạn trong báo cáo
   - (c) dùng SisFall/MobiFall làm tập kiểm chứng riêng (không train) — trái quyết định mục 5
2. Có cần ngoại vi cảnh báo không (buzzer / LED / SIM / BLE)? Hiện mới tính xuất UART.
3. **Cảm biến đặt ở đâu trên người?** HAPT thu ở **thắt lưng**. Đeo cổ tay hay túi quần sẽ
   lệch phân bố đáng kể so với dữ liệu train.
