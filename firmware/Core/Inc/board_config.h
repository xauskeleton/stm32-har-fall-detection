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
#define BAT_FULL_V          8.40f    /* 4.2 V / cell */
#define BAT_EMPTY_V         6.40f    /* 3.2 V / cell - khong xa pin thap hon muc nay */
#define BAT_NONE_V          5.00f    /* duoi muc nay = khong cam pin, chi cap USB */
#define BAT_MEASURE_MS      500u     /* chu ky do pin */
#define BAT_SHOW_MS         3000u    /* nhan nut thi 5 LED sang trong 3 s */
#define BAT_LOW_PCT         10u      /* duoi muc nay LED1 nhap nhay */

/* ---- 5 LED bao pin tren board: LED1..LED5 = PC1..PC5, anode len 3V3 qua 120R
 * => keo LOW la SANG. */
#define BAT_LED_PORT        GPIOC
#define BAT_LED_PINS        { GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5 }

/* ---- Nut nhan: noi GND khi nhan, board KHONG co tro keo len -> bat pull-up trong chip */
#define BTN_BAT_PORT        GPIOC    /* X1 = KEY3: xem pin */
#define BTN_BAT_PIN         GPIO_PIN_11
#define BTN_MUTE_PORT       GPIOC    /* nut tron tren hop, song song X2 = KEY1: tat coi */
#define BTN_MUTE_PIN        GPIO_PIN_9
#define BTN_DEBOUNCE_MS     30u

/* ---- Coi chu dong: chan + vao PA8 (J3 chan 13), chan - vao GND */
#define BUZZER_PORT         GPIOA
#define BUZZER_PIN          GPIO_PIN_8

/* ---- LED RGB cuc am chung (chan chung vao GND, ghi 1 la sang).
 * Moi chan mau phai co tro noi tiep ~220-330R.
 * KHONG dung PC13..PC15: datasheet cam dung 3 chan nay cap dong cho LED. */
#define RGB_R_PORT          GPIOB    /* J3 chan 6 */
#define RGB_R_PIN           GPIO_PIN_4
#define RGB_G_PORT          GPIOB    /* J3 chan 5 */
#define RGB_G_PIN           GPIO_PIN_5
#define RGB_B_PORT          GPIOC    /* J3 chan 15 */
#define RGB_B_PIN           GPIO_PIN_8
#define RGB_BLINK_MS        250u     /* nhip nhay do khi te nga */

#endif /* BOARD_CONFIG_H */
