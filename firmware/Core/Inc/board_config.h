/* Toan bo chan va hang so phan cung cua board STM32 F4 STUDY KIT, gom mot cho.
 * Doi chan / doi pin thi CHI sua file nay. Chan lay theo STM32_F4_STUDY_KIT_SCHE.
 */
#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include "stm32f4xx_hal.h"

/* ---- Do pin (thang bao pin 6.2..8.0 V, xem BAT_FULL_V / BAT_EMPTY_V) ----
 * Pin -> XT60 -> diode SS54 (D2) -> VCC -> chia ap R1 9k / R4 1k -> LMV358 dem
 * -> R3 470 + C1 470n -> PA5 (ADC1 kenh 5).
 * => V_pin = V_PA5 * 10 + sut ap diode. */
#define BAT_ADC_PORT        GPIOA
#define BAT_ADC_PIN         GPIO_PIN_5
#define BAT_ADC_CHANNEL     5u
#define BAT_DIVIDER         10.0f
#define BAT_DIODE_V         0.33f    /* do thuc te tren SS54: 7.74 V - 0.7406*10 = 0.334 V */
#define BAT_FULL_V          8.00f    /* 100% */
#define BAT_EMPTY_V         6.20f    /* 0%; duoi muc nay LED do nhay */
#define BAT_NONE_V          5.50f    /* duoi muc nay = khong cam pin, chi cap USB
                                      * (luc cam USB, VCC = 4.77 V -> g_bat_v ~ 5.10 V) */
#define BAT_MEASURE_MS      500u     /* chu ky do pin */
#define BAT_SHOW_MS         3000u    /* nhan nut thi LED RGB bao pin trong 3 s */
/* Mau LED RGB theo % pin (6.2 V = 0%, 8.0 V = 100%, 10 mau sang lien tuc moi 10%):
 *   91..100% (> 7.82 V)    trang           81..90% (7.64..7.82 V)  xanh ngoc (cyan)
 *   71..80%  (7.46..7.64 V) luc bao        61..70% (7.28..7.46 V)  xanh la
 *   51..60%  (7.10..7.28 V) non chuoi      41..50% (6.92..7.10 V)  vang
 *   31..40%  (6.74..6.92 V) cam vang       21..30% (6.56..6.74 V)  cam dam
 *   11..20%  (6.38..6.56 V) hong tim       0..10%  (6.20..6.38 V)  do
 *   < 6.20 V                do nhay nhanh (250 ms), LUON bao ke ca khi khong nhan nut
 *   < 5.50 V (chi USB)      xanh duong */

/* ---- Nut nhan: noi GND khi nhan, board KHONG co tro keo len -> bat pull-up trong chip */
#define BTN_BAT_PORT        GPIOC    /* X2 = KEY1, song song nut tron tren hop: xem pin */
#define BTN_BAT_PIN         GPIO_PIN_9
#define BTN_DEBOUNCE_MS     30u

/* ---- Coi chu dong: chan + vao PA8 (J3 chan 13), chan - vao GND */
#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_PIN_8

/* ---- LED RGB bao pin, cuc am chung (chan chung vao GND, ghi 1 la sang).
 * 5 LED PC1..PC5 tren board khong dung nua (MX_GPIO_Init van giu chung o muc 1 = tat).
 * Moi chan mau phai co tro noi tiep ~220-330R.
 * KHONG dung PC13..PC15: datasheet cam dung 3 chan nay cap dong cho LED. */
#define RGB_R_PORT          GPIOB    /* J3 chan 7 */
#define RGB_R_PIN           GPIO_PIN_3
#define RGB_G_PORT          GPIOB    /* J3 chan 6 */
#define RGB_G_PIN           GPIO_PIN_4
#define RGB_B_PORT          GPIOB    /* J3 chan 5 */
#define RGB_B_PIN           GPIO_PIN_5
#define RGB_BLINK_MS        250u     /* nhip nhay do khi sap het pin */

/* ---- Bluetooth HC-05 qua UART4. PB1 cap nguon, hien LUON BAT (PB1 = 1).
 * Muon tiet kiem pin thi co the dung PB1 tat HC-05 khi khong can (mach duoi).
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
#define BT_BAUD             115200u  /* baud rate UART4 giao tiep HC-05 */
#define BT_BOOT_MS          1000u    /* cho HC-05 khoi dong sau khi cap nguon */
#define BT_SEND_MS          2000u    /* chu ky gui hanh dong + trang thai */
#define BT_WINDOW_LOG       1        /* 1 = gui them 1 dong "W ..." MOI cua so (1.28 s) de
                                      * thay du chuoi nhan khi thu nga; 0 = tat */

#endif /* BOARD_CONFIG_H */
