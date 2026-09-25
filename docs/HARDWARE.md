# Phần cứng thực tế

Mọi số liệu dưới đây **đọc ra từ project firmware đã build thật**, không phải ước lượng:
cấu hình CubeMX (`test.ioc`), code khởi tạo (`Core/Src/main.c`, `icm20948.c`), và
linker map của một lần build (`Debug/test.map`). Nguồn ghi rõ ở từng mục.

Firmware nằm ở branch `Embeded_System` (do Quang dựng bằng STM32CubeIDE).

## Vi điều khiển

| | Giá trị | Nguồn |
|---|---|---|
| Chip | **STM32F446RET6** | `test.ioc: Mcu.CPN` |
| Lõi | Cortex-M4F, FPU single-precision | — |
| SYSCLK = HCLK | **180 MHz** (tối đa của F446) | `test.ioc: SYSCLKFreq_VALUE` |
| Nguồn clock | HSE 8 MHz → PLL (M=4, N=180) | `test.ioc: HSE_VALUE, PLLM, PLLN` |
| APB1 | 45 MHz (HCLK/4), timer 90 MHz | `test.ioc: APB1CLKDivider` |
| APB2 | 90 MHz (HCLK/2) | `test.ioc: APB2CLKDivider` |
| Flash | 512 KB @ `0x08000000` | `Debug/test.map` |
| SRAM | 128 KB @ `0x20000000` | `Debug/test.map` |

## Cảm biến ICM-20948

| | Giá trị | Nguồn |
|---|---|---|
| Giao tiếp | **I2C1, 400 kHz** (Fast Mode) | `main.c: hi2c1.Init.ClockSpeed` |
| Địa chỉ | `0xD0` (dạng 8-bit) = **0x68** 7-bit, chân AD0/SDO nối thấp | `icm20948.h: ICM20948_ADDR_L` |
| Thang gia tốc | **±4 g** → chia `8192` ra đơn vị g | `main.c:123 ACCEL_FS_SEL_4g` |
| Thang con quay | **±1000 °/s** → chia `32.768` rồi × π/180 ra rad/s | `main.c:124 GYRO_FS_SEL_1000` |
| Từ kế AK09916 | **không dùng** (HAPT không có từ kế) | — |
| Chuyển bank thanh ghi | đã xử lý (`ICM20948_ChangeUserBank`) | `icm20948.c:68` |

Hai thang đo này **đúng**, nhưng lý lẽ trong các bản trước của tài liệu này thì **sai**.
Bản trước ghi "dữ liệu HAPT lên tới 3,08 g và 629 °/s" — hai con số đó đo trên bản HAPT đã bị
cộng nhiễu σ ≈ 0,30. Trên bản gốc, đỉnh chỉ là **2,010 g** và **573,4 °/s**.

Quan trọng hơn: **HAPT đã bị bão hoà ngay từ điện thoại.** Giá trị `2,009722 g` lặp lại y hệt
11 lần và `10,008416 rad/s` (= 573,0 °/s) lặp lại 24 lần, trên cả 3 trục, tại đúng mốc tròn
2 g và 10 rad/s — Galaxy S II được đặt ở ±2 g / ±10 rad/s, và HAPT chứa **42 mẫu acc + 41 mẫu
gyro bị cắt ngọn thật**. Nghĩa là HAPT **không cho biết đỉnh thật** của chuyển động người,
nên "0 mẫu bị cắt ngọn ở ±4 g" là điều hiển nhiên chứ không phải bằng chứng.

Lý lẽ đúng cho ±4 g / ±1000 °/s:

| | lý do |
|---|---|
| **acc ±4 g** | ±2 g **dùng không được**: ngưỡng va đập của bộ phát hiện té ngã là **2,0 g**, cảm biến bão hoà ở 2 g thì không bao giờ báo được "> 2 g" → nhánh sốc chết hẳn. Té ngã ở thắt lưng thường 3–6 g |
| **gyro ±1000 °/s** | ngay trên dữ liệu **đã bị cắt ở 573 °/s** mà ±500 vẫn cắt 88 mẫu; đỉnh thật còn cao hơn |
| **không cần thang nhỏ hơn** | 16 bit cho 0,00012 g/LSB và 0,031 °/s/LSB, trong khi sàn nhiễu ICM-20948 ~0,0016 g và ~0,1 °/s ở 50 Hz — **nhiễu lớn hơn bước lượng tử hơn 10 lần**. Hạ thang đo không đo thêm được gì, chỉ mất khoảng dự trữ |

**Hệ quả phải xử lý trong firmware:** model train trên dữ liệu cắt ở 2 g / 10 rad/s, còn board
ở ±4 g / ±1000 dps sẽ báo giá trị vượt hai mốc đó. Nên **kẹp** acc vào ±2,0 g và gyro vào
±10 rad/s **cho kênh đặc trưng phân loại**, còn kênh phát hiện sốc dùng giá trị **không kẹp**
vì nó cần biết > 2 g.

> **Cần kiểm tra:** `main.c` ghi kèm bit `ACCEL_FCHOICE` / `GYRO_FCHOICE`, tức bật bộ
> lọc thông thấp trên chip (DLPF), còn các bit `DLPFCFG` để mặc định 0. Cần đối chiếu
> datasheet xem băng thông thực tế là bao nhiêu — nếu nó lọc đáng kể thì tín hiệu trên
> board sẽ mượt hơn dữ liệu HAPT dùng để train.

## Lấy mẫu

| | Giá trị | Nguồn |
|---|---|---|
| Tần số | **50 Hz chính xác** | `main.c:383` TIM6 |
| Cách tạo | TIM6: 90 MHz / 9000 / 200 = 50,000 Hz | `PSC = 8999, ARR = 199` |

Khớp đúng `fs = 50 Hz` của HAPT, không có sai số tần số.

## Ngoại vi khác

| | Giá trị | Nguồn |
|---|---|---|
| UART | USART1, chế độ async (in nhãn + trạng thái) | `test.ioc: USART1.VirtualMode` |
| LED trạng thái | **PC1, PC2, PC3** | `main.c:201-207` |
| Chân ra khác | PC4, PC5 | `test.ioc` |
| Chân vào | PC9, PC10, PC11, PC12 | `test.ioc` |

Ba LED báo ba trạng thái của state machine: `IDLE` → `SHOCK` → `FALLEN`.

## Bộ nhớ thực tế đã dùng

Từ `Debug/test.map` — số liệu của một lần build thật, không phải ước lượng.

| Vùng | Dùng | Có | Tỷ lệ |
|---|---|---|---|
| **Flash** | **161,7 KB** | 512 KB | **31,6%** |
| **RAM** | **~15,2 KB** | 128 KB | **11,9%** |

Chi tiết flash: `.text` 36,9 KB + `.rodata` 124,6 KB + `.data` 160 B
Chi tiết RAM: `.bss` 13,5 KB + `.data` 160 B + heap 512 B + stack 1 KB

### Thứ gì chiếm flash — kết quả bất ngờ

| File | Flash | Ghi chú |
|---|---|---|
| `arm_common_tables.o` | **117,0 KB** | **bảng tra FFT của CMSIS-DSP** |
| `model_params.o` | 6,8 KB | model cũ (12×120). Bản 160 đặc trưng: **~9,3 KB** |
| `arm_cfft_f32.o` | 4,4 KB | |
| `stm32f4xx_hal_i2c.o` | 4,1 KB | |
| `arm_cfft_radix8_f32.o` | 4,0 KB | |
| `icm20948.o` | 2,8 KB | |
| `main.o` | 2,1 KB | |

**Bảng tra FFT tốn 117 KB — gấp 17 lần bản thân model (6,8 KB).** CMSIS-DSP nhúng bảng
twiddle cho **mọi** kích thước FFT, dù ta chỉ cần 128 điểm.

Hiện vẫn vừa thoải mái (31,6% flash) nên không phải vấn đề. Nhưng nếu sau này cần chỗ,
đây là nơi cắt được nhiều nhất — hoặc giới hạn bảng CMSIS về đúng kích thước 128, hoặc
tự viết FFT radix-2 128 điểm (khoảng 60 dòng).

Cũng đáng lưu ý: **model nhỏ tới mức không còn là yếu tố nào trong bài toán bộ nhớ.**
Quyết định chọn LogisticRegression thay vì SVM (1,2 MB) vẫn đúng, nhưng lý do "để vừa
flash" giờ nên nói lại cho chính xác: SVM không vừa, còn giữa các model nhỏ thì flash
không phải tiêu chí phân biệt.

> **Lỗi trong template CubeIDE:** `STM32F446RETX_FLASH.ld` ghi
> `_Min_Heap_Size = 0x200; /* required amount of heap (4KB) */` và
> `_Min_Stack_Size = 0x400; /* required amount of stack (16KB) */`.
> Hai chú thích đều **sai**: 0x200 = 512 byte, 0x400 = 1 KB. Đây là chú thích sẵn của
> template ST, không phải lỗi của ai. Stack 1 KB hiện đủ vì mọi buffer lớn trong
> `har_dsp.c` đều khai báo `static` (nằm ở `.bss`), không nằm trên stack.

## Hướng đặt cảm biến — CHƯA khớp

Đọc từ log UART thật của board, lúc người đứng yên:

```
Hanh dong: STANDING | Accel: X=-1.00, Y=-0.02, Z=0.02
Hanh dong: STANDING | Accel: X=-1.00, Y=0.00, Z=0.01
```

Trọng lực nằm trên **−X**. Nhưng HAPT (dữ liệu dùng để train) có trọng lực trên **+X**
khi đứng: trung bình toàn dataset là x = +0,819, y = −0,008, z = +0,082.

Và khi nằm, board cho trọng lực trên **+Z**, còn HAPT có trọng lực trong mặt phẳng y–z
(y = +0,638, z = +0,541).

Thử cả 24 phép hoán vị trục vuông góc, có **3 phép** cho đúng cả `STANDING` lẫn `LAYING`,
nhưng chỉ **một** phép khớp HAPT vì lý do đúng:

```c
/* Hoan vi (-x, +z, +y). PHAI ap dung cho CA acc va gyro, neu khong thi
   40 dac trung nhom gyro sai het. */
float ax = -(float)raw_ax / 8192.0f;
float ay =  (float)raw_az / 8192.0f;
float az =  (float)raw_ay / 8192.0f;
float gx = -(float)raw_gx * 5.32105e-4f;
float gy =  (float)raw_gz * 5.32105e-4f;
float gz =  (float)raw_gy * 5.32105e-4f;
```

Chưa áp dụng — cần xác nhận board còn gắn nguyên như lúc lấy log.

**Vì sao phải sửa dù log trông có vẻ chạy đúng:** mô phỏng cho thấy ở hướng −X model đoán
`STANDING` với độ tin cậy **99,4%**, còn ở hướng +X đúng chuẩn chỉ **55,6%**. Model tự tin
hơn ở hướng sai, vì (−1,0,0) nằm ngoài vùng dữ liệu train nên model tuyến tính ngoại suy
ra biên rất xa. Tự tin cao trên đầu vào sai là tình huống không có gì cảnh báo.

Đo trên toàn bộ 24 hướng: chỉ **1/24** hướng cho accuracy ≥ 80%, trung bình 30,9%,
tệ nhất 4,3%.

## Thời gian tính toán

Chưa đo trên board, và **chưa có ước lượng cho bộ 552 đặc trưng**.

Con số cũ (~0,44 ms, có thể lên ~5 ms vì `calc_median` dùng insertion sort O(N²)) là cho bộ
120 đặc trưng. Bộ mới nặng hơn — phần đắt nhất là `entropy` (33 kênh) và sắp xếp cho `iqr`,
còn `arCoeff` Burg-4 (20 kênh) thì rẻ hơn cả FFT.

Ngân sách là **1.280 ms** (cứ 64 mẫu mới mới phân loại một lần). Ngay cả khi bộ mới đắt gấp
10 lần bộ cũ thì vẫn dưới **4% CPU**, nên đây không phải rủi ro — nhưng **phải đo thật** bằng
`DWT->CYCCNT` khi có board, đừng ghi số ước lượng vào báo cáo.

RAM: buffer tín hiệu cho 5 nhóm × 3 trục × 128 mẫu × 4 byte = **7,5 KB**, cộng ~1 KB cho Burg
và ~1 KB scratch FFT. Tổng RAM dự kiến ~20 KB / 128 KB.

## Toolchain

| | |
|---|---|
| IDE | **STM32CubeIDE** (`.cproject`, `test.ioc`, `Debug/`) |
| Thư viện | STM32F4 HAL + **CMSIS-DSP** (`arm_math.h`) |
| Ngôn ngữ | C, `float` toàn bộ (không `double`) |

CMSIS-DSP dùng cho: `arm_rfft_fast_f32` (FFT 128 điểm), `arm_biquad_cascade_df1_f32`
(lọc gravity), `arm_mean_f32` / `arm_std_f32` / `arm_max_f32` / `arm_min_f32` /
`arm_rms_f32` / `arm_dot_prod_f32`.

## Việc còn lại về phần cứng

- [ ] Áp dụng hoán vị trục `(-x, +z, +y)` cho cả acc và gyro
- [ ] Đo thời gian 1 cửa sổ bằng `DWT->CYCCNT`
- [ ] Đối chiếu datasheet về bit `FCHOICE` / `DLPFCFG` của ICM-20948
- [ ] Xác định baud rate UART (chưa tìm thấy trong code đã đọc)
- [ ] Cân nhắc cắt 117 KB bảng FFT nếu cần chỗ

## Hướng trục — lỗi hay gặp nhất khi đưa lên board mới

HAPT thu bằng điện thoại kẹp ở **thắt lưng**, và hướng gravity là thứ quyết định tư thế:

| tư thế | gravity trung bình (g) | trục "xuống" |
|---|---|---|
| **STANDING** | (+1,001, −0,166, −0,021) | **+X** |
| SITTING | (+0,957, +0,129, +0,146) | +X |
| WALKING | (+0,998, −0,180, −0,060) | +X |
| WALKING_UPSTAIRS | (+0,953, −0,269, −0,148) | +X |
| WALKING_DOWNSTAIRS | (+0,996, −0,153, −0,055) | +X |
| **LAYING** | (+0,069, +0,638, +0,541) | **+Y** (X ≈ 0) |

Năm trong sáu lớp có gravity trên **+X**; chỉ LAYING là X ≈ 0. Nghĩa là **nếu gravity không rơi
vào +X lúc đứng yên, model đọc thành LAYING bất kể người đang đứng** — và đó đúng là triệu chứng
gặp khi thử trên board lần đầu.

Đây **không** phải lỗi model: nó là lệch hệ trục giữa board và điện thoại của HAPT.
`har_app.h` có bảng `HAR_AXIS_SRC_*` / `HAR_AXIS_SIGN_*` kèm quy trình hiệu chuẩn 4 bước, và
`main.c` in cả `acc tho` (trước hoán vị) lẫn `gravity` (sau hoán vị) ra UART để hiệu chuẩn
không cần nạp lại firmware nhiều lần. Đứng yên thì `gravity` phải ra `(+1.0, ~0, ~0)`.
