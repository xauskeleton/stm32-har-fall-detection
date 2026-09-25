# Tiến độ

## Đã xong — Python

**Thư viện** (`har/`) — cố định vào đúng cấu hình đã chốt, không còn núm điều khiển thí nghiệm:

- `config.py` — hằng số DSP, cấu trúc 516 đặc trưng, 12 nhãn, chia train/test theo người
- `dsp.py` — median 3, lowpass 20 Hz, tách gravity 0,3 Hz, jerk (sai phân **lùi**), cắt cửa sổ,
  **516 đặc trưng**
- `dataset.py` — đọc HAPT (có cache .npz), Phyphox CSV, log UART, ground truth
- `fall.py` — state machine IDLE → SHOCK → FALLEN
- `model.py` — chọn cột + scaler + clf, metrics (có `fall_f1`), lưu/nạp bundle
- `export_c.py` — sinh `model_params.{h,c}` + `golden_vectors.h`

**Script** — `train.py` (có RFE), `eval_fall.py`, `export_model.py`

**Kết quả** (tập test 9 người chưa từng thấy, 3.162 cửa sổ, chấm một lần):

- LogisticRegression **160 → 12**, C=0.1 — **93,23%** / macro-F1 **85,81** / F1 4 nhãn chuyển
  tư thế **69,16**, 1.932 tham số (7,5 KB)
- Cross-validation 7 fold trong tập train: **94,69% ± 1,92** / macro-F1 **87,73** /
  F1 4 nhãn ngã **75,32**
- Gộp nhãn theo mức state machine dùng: **99,30%**
- Báo động sai té ngã trên 372 phút HAPT: **0** với nhãn oracle, **0** với nhãn từ model
- Công thức đặc trưng đã kiểm chứng **|r| = 1,000000 ở cả 15 phép** với bản 561 chính thức

## Đã xong — firmware C

Toàn bộ `float`, không `malloc` trong đường chạy, biên dịch `-Wall -Wextra -Wdouble-promotion`
**0 warning**. Đối chiếu với Python trên toàn bộ tập test: **0 / 3.156 cửa sổ lệch nhãn**,
accuracy giống hệt (91,73% cả hai — xem `DECISIONS.md` mục 7 về con số này).

- [x] `har_dsp.c` — median 3 streaming (trễ 1 mẫu, khớp bit với `har/dsp.py`), cascade biquad
      DF2T với state khởi tạo về xác lập, FFT radix-2 128 điểm tự viết, 14 phép thống kê
      (`iqr` quy ước MATLAB, `entropy` 256 ô cố định, `burg` AR bậc 4, …)
- [x] `har_features.c` — 516 đặc trưng, thứ tự khớp `har/dsp.py extract_features()`
- [x] `har_classifier.c` — chọn cột theo `HAR_SELECTED` → chuẩn hoá → nhân ma trận 160×12 → argmax
- [x] `har_pipeline.c` — ring buffer 5 nhóm × 3 trục × 128 mẫu, chạy mỗi 64 mẫu mới, giữ state
      bộ lọc qua các mẫu
- [x] `fall_detect.c` — bản sao 1-1 của `har/fall.py`, có cả `upright_min`
- [x] `test/test_parity.c` + `Makefile` — `make -C firmware test`, tiêu chí ĐẠT là **nhãn khớp
      từng cửa sổ**; `GOLDEN_TOL = 0.3` chỉ để chẩn đoán

## Đã xong — project STM32CubeIDE

`firmware/` giờ là project CubeIDE đầy đủ, mở trực tiếp bằng CubeIDE (`test.ioc`). Khung phần
cứng copy từ branch `Embeded_System` — bản đó **đã build và chạy trên board thật**, nên không
sửa lại, chỉ thay phần thuật toán.

- [x] `icm20948.c` + `.h` — driver I2C1 400 kHz, địa chỉ 0x68, có xử lý chuyển bank thanh ghi
- [x] `main.c` — CubeMX sinh (clock 180 MHz, GPIO, I2C1, USART1, TIM6 50 Hz), chỉ sửa trong các
      khối `USER CODE`: đọc cảm biến → đổi đơn vị (±4 g → g, ±1000 dps → rad/s) →
      `har_app_on_sample()` → in UART + LED theo trạng thái (PC1 xanh IDLE, PC2 vàng SHOCK,
      PC3 đỏ FALLEN)
- [x] `stm32f4xx_it.c` — ngắt TIM6 đặt cờ lấy mẫu, có đếm mẫu bị bỏ
- [x] HAL + CMSIS core, startup, linker script, `.cproject` / `.project` / `.ioc`
- [x] **Bỏ `Drivers/CMSIS_DSP`** — code mới tự viết FFT nên không dùng nữa. Tiết kiệm 559 file /
      13,9 MB trong repo, và **~117 KB flash** (bảng tra twiddle của CMSIS-DSP là thứ chiếm
      flash lớn nhất, xem `HARDWARE.md`). Đã xoá 30 chỗ nhắc `CMSIS_DSP` trong `.cproject`.
- [x] **Bỏ khởi tạo từ kế AK09916** — HAPT chỉ có acc + gyro, thêm từ kế vào lúc inference sẽ
      tạo đặc trưng mà model chưa từng thấy

Kiểm chứng được từ máy không có toolchain ARM: `gcc -fsyntax-only` với đúng include của HAL cho
**0 lỗi** trên `main.c`, `icm20948.c`, `stm32f4xx_it.c`, `stm32f4xx_hal_msp.c`,
`system_stm32f4xx.c`. Tức mọi symbol và chữ ký hàm khớp — **nhưng chưa build cho ARM và chưa
chạy trên board.**

## Chưa làm — cần board và CubeIDE

- [ ] **Build bằng CubeIDE** và lấy số liệu thật: flash / RAM / thời gian 1 cửa sổ bằng
      `DWT->CYCCNT`. Không làm được từ đây: máy không có `arm-none-eabi-gcc`.
- [ ] **Xác nhận hoán vị trục** `(−x, +z, +y)` còn đúng — phụ thuộc board hiện gắn thế nào.
      Đặt `HAR_AXIS_PERMUTE 0` trong `har_app.h` để tắt.
- [ ] Chốt bus cảm biến: I2C (đang dùng) hay SPI — nếu đổi thì chỉ sửa `read_reg`/`write_reg`
- [x] **Kẹp tín hiệu** — `har_app.c` kẹp **từng trục** vào ±2,0 g và ±10 rad/s trước khi đưa
      vào pipeline. HAPT bị bão hoà ở đúng hai mốc đó nên model chưa từng thấy giá trị vượt quá.
      Kẹp từng trục **không** làm chết nhánh phát hiện sốc: ngưỡng 2,2 g đo trên chuẩn
      `|a| = sqrt(x²+y²+z²)`, mà chuẩn lên tới 2√3 = 3,46 g dù mỗi trục bị kẹp ở 2 g — đúng
      như dữ liệu HAPT (bão hoà từng trục, vẫn có |a| tới 2,43 g ở phân vị 99 khi đi lại).
- [x] Hoán vị trục — `har_app.c` áp `(−x, +z, +y)` cho **cả acc và gyro** (`HAR_AXIS_PERMUTE`).
      **PHẢI xác nhận board còn gắn đúng như lúc ghi dữ liệu** trước khi tin bộ số này; đặt
      `HAR_AXIS_PERMUTE 0` để tắt.
- [ ] Ngân sách thời gian là 1.280 ms nên không có rủi ro, nhưng **chưa đo** cho bộ 516 đặc
      trưng. RAM dự kiến: buffer tín hiệu 7,5 KB + kênh sốc 1,5 KB + scratch ~2 KB.

## Chưa làm — phần Python

- [ ] `scripts/infer_file.py` — suy luận offline trên file đo tay + chạy state machine té ngã,
      in chuỗi nhãn theo thời gian và sự kiện té ngã
- [ ] Copy vài sample đo tay từ `../DSP/test/my_samples/` sang `test/my_samples/`
- [ ] Sửa `har/fall.py` nhánh chết trong `STATE_IDLE`: `_laying` tối đa bằng 1 nhưng so với
      `laying_confirm = 2` nên không bao giờ đúng. Quyết định bỏ hay sửa.

## Câu hỏi còn treo

1. **Không có mẫu té ngã thật nào** — đây là nút chặn lớn nhất.
   Mọi số liệu hiện có chỉ chứng minh *không báo động sai*, chưa chứng minh *phát hiện được*.
   Nặng hơn nữa: `impact_g` đã phải nâng 2,0 → **2,2 g** để bù cho việc bật `median 3 +
   lowpass 20 Hz` (`DECISIONS.md` mục 4), tức **độ nhạy đã bị giảm một lượng chưa đo được**.
   Ba hướng:
   - (a) tự đo: đẩy board/điện thoại xuống đệm, viết `.gt.txt` **không có** nhãn transition
   - (b) chỉ đánh giá phía âm, nói rõ giới hạn trong báo cáo
   - (c) dùng SisFall/MobiFall **chỉ làm tập test**, không tham gia huấn luyện — dùng kiểu đó
     **không** trái quyết định mục 1
2. Có cần ngoại vi cảnh báo không (buzzer / LED / SIM / BLE)? Hiện mới tính xuất UART.
3. **Cảm biến đặt ở đâu trên người?** HAPT thu ở **thắt lưng**. Đeo cổ tay hay túi quần sẽ
   lệch phân bố đáng kể so với dữ liệu train.
4. **Ba rủi ro mà không split nào của HAPT phủ được** — hướng gắn, bão hoà, sàn nhiễu (train
   0,022 g vs ICM-20948 ~0,0016 g, lệch ~14 lần). Cross-validation cũng không bắt được vì nó
   chỉ so trong cùng một bộ dữ liệu. Cách phát hiện: ghi ~10 phút dữ liệu thật từ board, cho
   qua đúng pipeline đặc trưng, rồi **so histogram từng đặc trưng với histogram của tập train**.
   Rẻ, không cần nhãn, và bắt được đúng loại lỗi mà accuracy không bao giờ chỉ ra.
5. **`bandsEnergy` gần như là gánh nặng chết** — 126 cột tính ra chỉ dùng 7 (`DECISIONS.md`
   mục 3). Cắt được 119 cột nếu cần gọn, nhưng chi phí tính không đáng kể nên chưa làm.
