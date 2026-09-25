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

## Chưa làm — phần cứng (chờ có board)

- [ ] Chốt toolchain (khuyến nghị STM32CubeIDE — một bộ cài, có sẵn trình biên dịch + nạp)
- [ ] Chốt bus cảm biến: I2C hay SPI
- [ ] `icm20948.c` — driver, tách lớp `read_reg`/`write_reg` để đổi I2C↔SPI chỉ sửa 2 hàm.
      Cấu hình **±4 g / ±1000 °/s**, output 50 Hz.
      **Đổi đơn vị sang g và rad/s cho khớp HAPT.**
      Lý do thang đo: ±2 g dùng không được vì ngưỡng va đập là 2,2 g; ±500 °/s thì cắt ngọn.
      Xem `DECISIONS.md` mục 8 — con số "629 °/s" trong các bản trước là **sai**, nó đo trên
      dữ liệu đã bị cộng nhiễu.
- [ ] **Kẹp tín hiệu trước khi gọi `har_pipeline_push()`**: acc vào ±2,0 g, gyro vào ±10 rad/s.
      HAPT bị bão hoà ở đúng hai mốc đó (42 + 41 mẫu bị cắt ngọn) nên model chưa từng thấy giá
      trị vượt quá. Nếu muốn đo đỉnh va đập thật cho kênh sốc thì phải đưa thêm một đường
      **không kẹp** vào `har_pipeline.c`.
- [ ] Hoán vị trục cảm biến. Dữ liệu ghi từ board cho thấy gravity nằm trên −X khi đứng, còn
      HAPT cần +X → cần `(−x, +z, +y)` cho **cả acc và gyro**. Phải xác nhận board còn gắn
      đúng như lúc ghi trước khi áp.
- [ ] `board.c` — timer ngắt 50 Hz, UART, LED
- [ ] `main.c` — vòng lặp chính: đọc cảm biến → kẹp → `har_pipeline_push()` →
      `fall_update()` → xuất UART/LED
- [ ] Đo thực tế: thời gian 1 cửa sổ bằng `DWT->CYCCNT`, RAM/flash thật.
      Ngân sách là 1.280 ms nên không có rủi ro, nhưng **chưa đo** cho bộ 516 đặc trưng.
      RAM dự kiến: buffer tín hiệu 7,5 KB + kênh sốc 1,5 KB + scratch ~2 KB.

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
