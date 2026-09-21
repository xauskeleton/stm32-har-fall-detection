# Tiến độ

Cập nhật: 2026-09-21. Repo chưa `git init`, chưa commit, chưa push.

**Hướng hiện tại:** hoàn thiện phần nhận dạng (Python) trước, firmware làm sau.

## Đã xong

- [x] `har/config.py` — hằng số DSP, 12 nhãn, nhóm nhãn té ngã, split theo subject,
      thêm `GROUPS_MIN` / `GROUPS_FULL` + `n_features()`
- [x] `har/dsp.py` — chọn được nhóm tín hiệu, bật/tắt lọc, 2 chế độ `causal` / `zerophase`
- [x] `har/dataset.py` — `load_raw()` (có cache .npz) tách khỏi `build_windows()` để sweep
      nhiều cấu hình mà chỉ đọc đĩa một lần; loader Phyphox / UART / ground truth
- [x] `har/fall.py` — state machine IDLE → SHOCK → FALLEN
- [x] `har/model.py` — metrics / report / save-load bundle + `linear_weights()` cho export C
- [x] `scripts/train.py` — train + `--sweep` (5 bộ đặc trưng × 8 model **ML cổ điển**,
      không mạng nơ-ron: logreg, linsvc, lda, ridge, gnb, tree, rf50, svm_rbf)
- [x] **Đã chốt model: LogisticRegression 120→12, C=0.1, causal — 86.88% / macro-F1 80.69,
      1.452 tham số.** Chọn thay vì `linsvc` (accuracy cao hơn 0.37% nhưng macro-F1 thấp hơn 1.75)
      → `models/{scaler,classifier}.pkl` + `meta.json`, số liệu ở `models/sweep.json`
- [x] `scripts/eval_fall.py` — chạy state machine trên toàn bộ 372 phút HAPT, tách lỗi
      do bộ phân loại với lỗi do thiết kế state machine
- [x] **Sửa `har/fall.py` + `har/dsp.py`: thêm lọc median 3 cho kênh phát hiện sốc.**
      Nhiễu 0,30g/trục của HAPT làm 12,9% cửa sổ tư thế tĩnh thỏa điều kiện sốc → 77–120
      báo nhầm. Median 3 đưa về **0,0%** mà vẫn giữ 74,9% độ nhạy với lực đập thật.
      `shock_rule` giữ mặc định `'both'`. Còn **2 báo nhầm / 6,2 giờ** (0 nếu nhãn hoàn hảo)
- [x] `scripts/pseudo_fall.py` — thử nén thời gian. **Không kết luận được gì về tỷ lệ phát
      hiện té ngã** (nén làm giảm biên độ, `a_max` max 1,914g < ngưỡng 2,0g → nhánh sốc
      không bao giờ kích hoạt). Xem `docs/DECISIONS.md` mục 9.
- [x] `docs/DECISIONS.md` — số liệu chứng minh từng quyết định (7 mục)

## Đang làm

*(trống — chờ user chọn việc tiếp theo)*

## Chưa làm — phần Python (làm được ngay, không cần toolchain)

- [ ] `scripts/infer_file.py` — suy luận offline trên file HAPT/Phyphox + chạy state machine té ngã.
      Gộp `analyze.py` + `predict_hud.py` + `inference_simple.py` của repo DSP làm một.

- [ ] Copy sample từ `../DSP/test/my_samples/` sang `test/my_samples/`
- [ ] Sửa `har/fall.py:110-113` — nhánh chết: trong `STATE_IDLE`, `_laying` tối đa bằng 1
      nhưng so với `laying_confirm = 2` nên không bao giờ đúng. Quyết định bỏ hay sửa.
- [ ] `README.md` — mô tả đề tài, sơ đồ pipeline, hướng dẫn chạy

## Chưa làm — phần nhúng (chờ chốt toolchain)

- [ ] Chốt toolchain: STM32CubeIDE / arm-none-eabi + Makefile / PlatformIO
- [ ] Chốt bus cảm biến: I2C hay SPI
- [ ] `har/export_c.py` + `scripts/export_model.py` — sinh `model_params.{h,c}`:
      hệ số SOS 0.3 Hz, scaler mean/scale (120), ma trận logreg 12×120 + bias, ngưỡng té ngã,
      và `golden_vectors.h` để test đối chiếu
- [ ] `firmware/Core/Inc/har_config.h` — bản sao C của `har/config.py`
- [ ] `firmware/Core/Src/icm20948.c` — driver, tách lớp `read_reg`/`write_reg` để đổi I2C↔SPI.
      Cấu hình ±4g / ±500 dps, output 50 Hz. **Đổi đơn vị sang g và rad/s cho khớp HAPT.**
- [ ] **Toàn bộ firmware dùng `float` (32-bit), KHÔNG dùng `double`.** FPU của Cortex-M4
      chỉ có single precision; `double` bị emulate bằng software, chậm 50–100 lần.
      Đã đo: float32 cho **kết quả giống hệt** float64 — 0/3.530 cửa sổ khác nhãn.
      **Không cần quantize int8** (xem `docs/DECISIONS.md` mục 10).
- [ ] `median3.c` (lọc median 3 mẫu cho kênh sốc — xem `DECISIONS.md` mục 8)
- [ ] `dsp_filter.c` (biquad DF2T bậc 3, chỉ cho 0.3 Hz), `fft.c` (radix-2 128 điểm),
      `features.c` (120 đặc trưng, cần `median_f32` khớp `np.median`), `classifier.c`
      (nhân ma trận 120×12 + argmax), `fall_detect.c`, `har_pipeline.c`, `main.c`
- [ ] `firmware/test/test_parity.c` — đối chiếu C vs Python.
      **Dùng sai số TUYỆT ĐỐI < 1e-3, không dùng sai số tương đối.** Đã đo trên 3.530 cửa sổ:
      sai số tuyệt đối trung bình 1,2e-05, lớn nhất 6,4e-03 (ở `fft_energy`, biên độ lớn).
      Tiêu chí tương đối sẽ **fail oan** ở `body_acc_*_mean` — đặc trưng này vốn ≈ 0 nên
      sai số tương đối lên tới 1,96 dù tuyệt đối chỉ 1,7e-05.
- [ ] Đo thực tế: thời gian 1 cửa sổ trên F446 @180 MHz, RAM/flash thật

## Câu hỏi còn treo

1. **Không có mẫu té ngã thật nào.** `chuoi_nga.gt.txt` bên repo DSP — dù tên là "chuỗi ngã" —
   có ground truth ghi `23.00 24.00 STAND_TO_LIE`, tức nằm xuống **chủ động**, nên nó là
   negative test case chứ không phải positive. Ba hướng:
   - (a) tự đo: đẩy board/điện thoại xuống đệm, viết `.gt.txt` **không có** nhãn transition
   - (b) chỉ đánh giá phía negative (không báo động nhầm khi nằm chủ động), nói rõ giới hạn
   - (c) dùng SisFall/MobiFall làm tập kiểm chứng riêng (không train) — trái quyết định #1
2. Có cần ngoại vi cảnh báo không (buzzer / LED / SIM / BLE)? Hiện mới tính xuất UART.
3. ICM-20948 đặt ở đâu trên người? HAPT thu ở **thắt lưng**. Đeo cổ tay hay túi quần sẽ
   lệch phân bố đáng kể so với dữ liệu train.
