/* Toan bo chan va hang so phan cung cua board STM32 F4 STUDY KIT, gom mot cho.
 * Doi chan / doi pin thi CHI sua file nay. Chan lay theo STM32_F4_STUDY_KIT_SCHE.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* ---- Do pin: 2 cell 18650 noi tiep (2S) ----
 * Pin -> XT60 -> diode SS54 (D2) -> VCC -> chia ap R1 9k / R4 1k -> LMV358 dem
 * -> R3 470 + C1 470n -> PA5 (ADC1 kenh 5).
 * => V_pin = V_PA5 * 10 + sut ap diode. */
#define BAT_ADC_PORT        GPIOA
#define BAT_ADC_PIN         GPIO_PIN_5
#define BAT_ADC_CHANNEL     5u
#define BAT_DIVIDER         10.0f
#define BAT_DIODE_V         0.40f    /* SS54 o dong nho; do bang dong ho roi chinh */
#define BAT_FULL_V          7.60f    /* 100% */
#define BAT_EMPTY_V         4.00f    /* 0%; duoi muc nay LED do nhay */
#define BAT_NONE_V          3.00f    /* duoi muc nay = khong cam pin, chi cap USB
                                      * PHAI do lai: cap USB, khong cam pin, xem UART in bao nhieu V */
#define BAT_MEASURE_MS      500u     /* chu ky do pin */
#define BAT_SHOW_MS         3000u    /* nhan nut thi LED RGB bao pin trong 3 s */
/* Mau LED RGB theo % pin (4.0 V = 0%, 7.6 V = 100%), "nhay" = nhay cham BAT_BLINK_SLOW_MS:
 *   > 80%  (> 6.88 V)   xanh la          60..80%  xanh la nhay
 *   40..60%             vang             20..40%  vang nhay
 *   <= 20% (4.0..4.72 V) do
 *   < 4.0 V             do nhay nhanh, LUON bao ke ca khi khong nhan nut
 *   khong cam pin (chi USB)  xanh duong */
#define BAT_PCT_FULL        80u
#define BAT_PCT_GOOD        60u
#define BAT_PCT_HALF        40u
#define BAT_PCT_LOW         20u
#define BAT_BLINK_SLOW_MS   500u

/* ---- Nut nhan: noi GND khi nhan, board KHONG co tro keo len -> bat pull-up trong chip */
#define BTN_BAT_PORT        GPIOC    /* X1 = KEY3: xem pin */
#define BTN_BAT_PIN         GPIO_PIN_11
#define BTN_MUTE_PORT       GPIOC    /* nut tron tren hop, song song X2 = KEY1: tat coi */
#define BTN_MUTE_PIN        GPIO_PIN_9
#define BTN_DEBOUNCE_MS     30u

/* ---- Coi chu dong: chan + vao PA8 (J3 chan 13), chan - vao GND */
#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_PIN_8

/* ---- LED RGB bao pin, cuc am chung (chan chung vao GND, ghi 1 la sang).
 * 5 LED PC1..PC5 tren board khong dung nua (MX_GPIO_Init van giu chung o muc 1 = tat).
 * Moi chan mau phai co tro noi tiep ~220-330R.
 * KHONG dung PC13..PC15: datasheet cam dung 3 chan nay cap dong cho LED. */
#define RGB_R_PORT          GPIOB    /* J3 chan 6 */
#define RGB_R_PIN           GPIO_PIN_4
#define RGB_G_PORT          GPIOB    /* J3 chan 5 */
#define RGB_G_PIN           GPIO_PIN_5
#define RGB_B_PORT          GPIOB    /* J3 chan 7 - ba chan R/G/B lien nhau tren J3 */
#define RGB_B_PIN           GPIO_PIN_3
#define RGB_BLINK_MS        250u     /* nhip nhay do khi sap het pin */

/* ---- Bluetooth HC-05 qua UART4, nguon bat/tat bang 1 GPIO de tiet kiem pin.
 * HC-05 an 30-40 mA khi cho ghep noi -> binh thuong TAT, chi bat khi te nga
 * hoac khi nhan nut xem pin.
 *
 *   PB1 -- 1k --+-- B  NPN (S8050 / 2N3904)      +5V ---+---------+
 *               |      E -> GND                         |         |
 *             100k     C -------------------------------+-- 10k   S
 *               |                                       G <-------+
 *              GND                               P-MOSFET (AO3401)
 *                                                       D -> VCC HC-05
 *   PB1 = 1 -> NPN dan -> G keo xuong GND -> MOSFET dan -> HC-05 co nguon.
 *   100k giu NPN tat luc reset (chan dang la input tha noi).
 *
 *   CACH THU NHANH, khong can transistor: noi thang PB1 -> VCC HC-05, code giu
 *   nguyen. Chi de thu: GPIO chi ra 3.3 V va chiu toi da ~25 mA, trong khi HC-05
 *   an 30-40 mA luc cho ghep noi -> ap sut, module co the reset lien tuc, dung
 *   lau co the hong chan PB1. Ban dung that thi dung mach transistor o tren.
 *
 *   HC-05 TXD -> A1 (PA1, UART4_RX)     HC-05 RXD <- A0 (PA0, UART4_TX)
 *   HC-05 GND -> GND                    STATE, EN: bo trong */
#define BT_PWR_PORT         GPIOB
#define BT_PWR_PIN          GPIO_PIN_1
#define BT_UART             UART4 
#define BT_TX_PORT          GPIOA
#define BT_TX_PIN           GPIO_PIN_0
#define BT_RX_PORT          GPIOA
#define BT_RX_PIN           GPIO_PIN_1
#define BT_GPIO_AF          GPIO_AF8_UART4
#define BT_BAUD             9600u    /* mac dinh cua HC-05 o che do du lieu */
#define BT_BOOT_MS          1000u    /* cho HC-05 khoi dong sau khi cap nguon */
#define BT_SEND_MS          2000u    /* chu ky gui trang thai khi dang bat */
#define BT_HOLD_MS          30000u   /* giu nguon them sau khi het te nga / nhan nut */

#endif /* BOARD_CONFIG_H */
