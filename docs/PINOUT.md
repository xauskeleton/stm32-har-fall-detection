# Sơ đồ chân — STM32F446RET6 (board STM32 F4 Study Kit)

Nguồn sự thật là `firmware/Core/Inc/board_config.h` và `firmware/test.ioc`. Đổi chân thì sửa
`board_config.h` rồi cập nhật lại file này.

`test.ioc` chỉ khai báo các chân CubeMX sinh code (I2C1, USART1, SWD, HSE, PC1–PC5, PC9–PC12).
Các chân PA0, PA1, PA5, PA8, PB1, PB3, PB4, PB5 do code tự khởi tạo trong `battery.c`,
`buzzer.c`, `rgb_led.c`, `bluetooth.c` — CubeMX không biết, nên đừng gán chúng cho ngoại vi khác
trong CubeMX.

## Chân cần đi dây ra ngoài

| Linh kiện | Chân linh kiện | Nối vào board | Chân MCU | Ghi chú |
|---|---|---|---|---|
| Còi chủ động | + | J3 chân 13 (A8) | PA8 | Output, mức 1 = kêu |
| | − | GND | | |
| LED RGB (cực âm chung) | Đỏ | J3 chân 7 (B3) | PB3 | Mỗi màu qua điện trở 220–330 Ω, ghi 1 = sáng |
| | Xanh lá | J3 chân 6 (B4) | PB4 | |
| | Xanh dương | J3 chân 5 (B5) | PB5 | |
| | Chân chung | GND | | |
| Bluetooth HC-05 | VCC | Header trái, B1 | PB1 | Output mức 1 = bật HC-05. Cấp thẳng từ GPIO chỉ để thử |
| | RXD | Header trái, A0 | PA0 | UART4_TX, 9600 baud |
| | TXD | Header trái, A1 | PA1 | UART4_RX |
| | GND | GND | | |
| Nút tròn ngoài hộp | 1 đầu | J3 chân 14 (C9) | PC9 | Input pull-up, nối song song KEY1. Nhấn = LED RGB báo pin 3 s |
| | Đầu kia | GND | | |
| Pin | | Cổng XT60 | | Mạch đo có sẵn trên board. Thang báo pin đang đặt 4,0–7,6 V (xem dưới) |

Nối UART phải bắt chéo: TX của STM32 vào RXD của HC-05, RX của STM32 vào TXD của HC-05.

## Chân dùng phần cứng có sẵn trên board

| Chân MCU | Chức năng | Ghi chú |
|---|---|---|
| PB8 / PB9 | I2C1 SCL / SDA | ICM-20948, 400 kHz, địa chỉ 0x68, ±4 g / ±1000 dps |
| PA9 / PA10 | USART1 TX / RX | Cổng USB UART, 115200 baud, in log |
| PA5 | ADC1 kênh 5 | Đo điện áp pin qua mạch chia 10 lần: V_pin = ADC × 3,3 / 4095 × 10 |
| PC9 | Nút KEY1 (X2) | Input pull-up, song song nút ngoài hộp. Nhấn = xem pin |
| PA13 / PA14 | SWDIO / SWCLK | Nạp code qua ST-Link — không dùng cho việc khác |
| PH0 / PH1 | Thạch anh HSE | 8 MHz, PLL lên 180 MHz |

Không còn dùng: PC1–PC5 (5 LED trên board, giữ ở mức tắt), PC10, PC11 (KEY3), PC12 (khai báo input, chưa dùng).

## Ngoại vi nội bộ

| Ngoại vi | Cấu hình |
|---|---|
| TIM6 | Ngắt 20 ms = lấy mẫu 50 Hz (PSC 8999, ARR 199) |
| UART4 | HC-05, luôn bật. Mỗi 2 s gửi `hành động \| trạng thái \| pin`, té ngã / đứng dậy thì gửi ngay |
| ADC1 | Đọc bằng thanh ghi, trung bình 16 mẫu, chu kỳ 500 ms |

## LED RGB báo pin

Lúc khởi động: tự kiểm tra đỏ → xanh lá → xanh dương (mỗi màu 0,4 s), rồi báo pin 3 s.
Sau đó tắt, nhấn nút ngoài hộp hoặc KEY1 (PC9) thì báo pin lại 3 s.

Thang đo: 4,0 V = 0%, 7,6 V = 100%.

| Màu | Mức pin | Điện áp |
|---|---|---|
| Xanh lá | > 80% | > 6,88 V |
| Xanh lá nhấp nháy chậm | 60–80% | 6,16–6,88 V |
| Vàng | 40–60% | 5,44–6,16 V |
| Vàng nhấp nháy chậm | 20–40% | 4,72–5,44 V |
| Đỏ | ≤ 20% | 4,0–4,72 V |
| Đỏ nhấp nháy nhanh | hết pin | < 4,0 V, luôn báo kể cả khi không nhấn nút |
| Xanh dương | Không cắm pin, chạy nguồn USB | < 3,0 V |

## Chân còn trống trên header

A2, A3, A4, A6, A7, B0, B2, B10, C0, C4, C10, C12, B6, B7, D2, A15, C6, C7, B12–B15.

Không dùng PC13–PC15 để cấp dòng cho LED (datasheet cấm).
