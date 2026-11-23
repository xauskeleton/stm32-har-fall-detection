/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "icm20948.h"
#include "sensor_app.h"
#include "board_config.h"
#include "battery.h"
#include "buzzer.h"
#include "buttons.h"
#include "bluetooth.h"
#include "uart_dma.h"
#include <math.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
volatile uint8_t flag_sample_imu = 0;
volatile uint32_t missed_sample_count = 0;

ICM20948_int16_vector3 accel_data;
ICM20948_int16_vector3 gyro_data;

/* Ket qua phan loai gan nhat, de in ra UART moi 2 giay */
static har_app_event_t last_ev = { 0, 0, 0, 0.0f, 0.0f };
static uint32_t n_fall_total = 0;
static float last_acc_raw[3] = { 0.0f, 0.0f, 0.0f };   /* truoc hoan vi, de hieu chuan */
static float last_gyro_dps[3] = { 0.0f, 0.0f, 0.0f };

#define U1_TXQ_SIZE 512u
static uint8_t u1_txq[U1_TXQ_SIZE];
static uart_dma_tx_t u1_dma = {
    .uart       = USART1,
    .dma        = DMA2,
    .stream     = DMA2_Stream7,
    .channel    = 4u,
    .stream_idx = 7u,
    .irqn       = DMA2_Stream7_IRQn,
    .buf        = u1_txq,
    .size       = U1_TXQ_SIZE,
};
static uint8_t u1_dma_ready = 0;

#define DEG2RAD 0.0174532925f
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void MX_TIM6_Init(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
int __io_putchar(int ch) {
    uint8_t c = (uint8_t)ch;
    if (u1_dma_ready) UART_DMA_Write(&u1_dma, &c, 1u);
    else              HAL_UART_Transmit(&huart1, &c, 1, HAL_MAX_DELAY);
    return ch;
}

int _write(int file, char *ptr, int len) {
    (void)file;
    if (len <= 0) return 0;
    if (u1_dma_ready) UART_DMA_Write(&u1_dma, ptr, (uint16_t)len);
    else              HAL_UART_Transmit(&huart1, (uint8_t *)ptr, (uint16_t)len, HAL_MAX_DELAY);
    return len;
}

void UART1_DMA_IRQHandler(void) {
    UART_DMA_IRQHandler(&u1_dma);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  UART_DMA_Init(&u1_dma);
  u1_dma_ready = 1;
  har_app_reset();
  Buzzer_Init();
  printf("RGB LED self-test: red -> green -> blue\r\n");
  Battery_Init();          /* khoi tao LED RGB (co tu kiem tra 3 mau) + do pin */
  Battery_Show();          /* khoi dong: LED RGB bao muc pin 3 s */
  Bluetooth_Init();        /* HC-05 luon bat, gui trang thai moi 2 s */

  static button_t btn_bat  = BUTTON_INIT(BTN_BAT_PORT, BTN_BAT_PIN);

  uint8_t i2c_ok = 0;
  if (ICM20948_Init(&hi2c1, ICM20948_SDO_LOW) == HAL_OK) {
      ICM20948_Wake();
      ICM20948_WriteRegister(&ICM20948_REG_ACCEL_CONFIG_1, ICM20948_ACCEL_FCHOICE | ICM20948_ACCEL_FS_SEL_4g);
      ICM20948_WriteRegister(&ICM20948_REG_GYRO_CONFIG_1, ICM20948_GYRO_FCHOICE | ICM20948_GYRO_FS_SEL_1000);
      i2c_ok = 1;
  } else {
      printf("Error: failed to read ICM-20948 over I2C\r\n");
  }
  /* KHONG khoi tao tu ke: HAPT thu bang dien thoai chi co acc + gyro, nen dua
   * them tu ke vao luc inference se tao ra dac trung ma model chua tung thay. */

  MX_TIM6_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* --- ngoai vi: khong ham nao chan, de khong lo nhip lay mau 50 Hz --- */
    /* Nut xem pin: KEY1 / nut tron ngoai hop (PC9) */
    if (Button_Pressed(&btn_bat)) Battery_Show();
    Battery_Update();                               /* do pin + mau LED RGB */
    Buzzer_Update();
    const fall_state_t *fs = har_app_fall_state();
    float gv[3];
    har_app_gravity(gv);

    /* Gui hanh dong + trang thai + toan bo tham so debug qua UART4 Bluetooth (DMA) */
    bt_telemetry_t bt = {
        .label      = last_ev.label,
        .fall_state = fs->state,
        .a_min      = last_ev.a_min,
        .a_max      = last_ev.a_max,
        .acc_raw    = last_acc_raw,
        .gyro_dps   = last_gyro_dps,
        .grav       = gv,
        .active_run = fs->active_run,
        .guard      = fs->guard,
        .laying     = fs->laying,
        .missed     = missed_sample_count,
        .bat_v      = Battery_GetVoltage(),
        .bat_pct    = Battery_IsPresent() ? (int)Battery_GetPercent() : -1,
        .bat_adc    = g_bat_adc_raw,
        .bat_pa5_v  = g_bat_pa5_v,
    };
    Bluetooth_Update(&bt);

    /* --- in trang thai + tham so debug ra USART1 (DMA) moi 2 giay --- */
    static uint32_t last_uart_time = 0;
    if (HAL_GetTick() - last_uart_time >= 2000) {
      last_uart_time = HAL_GetTick();
      const char *state_name = (fs->state == FALL_STATE_IDLE)  ? "IDLE"
                             : (fs->state == FALL_STATE_SHOCK) ? "SHOCK" : "FALLEN";
      char ax[10], ay[10], az[10], gx[10], gy[10], gz[10], amin[10], amax[10];
      UART_FmtF2(ax, sizeof ax, last_acc_raw[0], 1);
      UART_FmtF2(ay, sizeof ay, last_acc_raw[1], 1);
      UART_FmtF2(az, sizeof az, last_acc_raw[2], 1);
      UART_FmtF2(gx, sizeof gx, gv[0], 1);
      UART_FmtF2(gy, sizeof gy, gv[1], 1);
      UART_FmtF2(gz, sizeof gz, gv[2], 1);
      UART_FmtF2(amin, sizeof amin, last_ev.a_min, 0);
      UART_FmtF2(amax, sizeof amax, last_ev.a_max, 0);

      printf("%-18s | %-6s | acc(%s %s %s) | gyro(%+d %+d %+d dps)"
             " | grav(%s %s %s) | |a| %s..%s g | falls:%lu"
             " | sm(r=%d,g=%d,l=%d) | missed:%lu\r\n",
             har_label_name(last_ev.label), state_name,
             ax, ay, az,
             (int)lroundf(last_gyro_dps[0]), (int)lroundf(last_gyro_dps[1]),
             (int)lroundf(last_gyro_dps[2]),
             gx, gy, gz, amin, amax,
             (unsigned long)n_fall_total,
             fs->active_run, fs->guard, fs->laying,
             (unsigned long)missed_sample_count);

      char sbv[12];
      UART_FmtF2(sbv, sizeof sbv, Battery_GetVoltage(), 0);
      int pa5_mv = (int)lroundf(g_bat_pa5_v * 1000.0f);
      if (Battery_IsPresent()) {
        printf("Battery: %s V (%u%%) | adc=%lu pa5=%d.%03dV\r\n",
               sbv, (unsigned)Battery_GetPercent(),
               (unsigned long)g_bat_adc_raw, pa5_mv / 1000, pa5_mv % 1000);
      } else {
        printf("Battery: no battery (USB power) | raw=%s V adc=%lu pa5=%d.%03dV\r\n",
               sbv, (unsigned long)g_bat_adc_raw, pa5_mv / 1000, pa5_mv % 1000);
      }
    }

    /* --- moi mau o 50 Hz --- */
    if (flag_sample_imu) {
      flag_sample_imu = 0;

      if (i2c_ok) {
          ICM20948_ReadAccelGryoRegisters(&accel_data, &gyro_data);
      } else {
          /* I2C hong -> gia lap dung yen de firmware van chay duoc */
          accel_data.x = 0; accel_data.y = 0; accel_data.z = 8192;
          gyro_data.x = 0; gyro_data.y = 0; gyro_data.z = 0;
      }

      /* Thang do +-4 g va +-1000 dps -> doi ra g va rad/s cho khop HAPT.
       * Hoan vi truc va kep tin hieu do har_app lo, khong lam o day. */
      float acc[3], gyro[3];
      acc[0] = (float)accel_data.x / 8192.0f;
      acc[1] = (float)accel_data.y / 8192.0f;
      acc[2] = (float)accel_data.z / 8192.0f;
      gyro[0] = ((float)gyro_data.x / 32.768f) * DEG2RAD;
      gyro[1] = ((float)gyro_data.y / 32.768f) * DEG2RAD;
      gyro[2] = ((float)gyro_data.z / 32.768f) * DEG2RAD;

      last_acc_raw[0] = acc[0];
      last_acc_raw[1] = acc[1];
      last_acc_raw[2] = acc[2];
      last_gyro_dps[0] = (float)gyro_data.x / 32.768f;
      last_gyro_dps[1] = (float)gyro_data.y / 32.768f;
      last_gyro_dps[2] = (float)gyro_data.z / 32.768f;

      har_app_event_t ev;
      if (har_app_on_sample(acc, gyro, &ev)) {
        last_ev = ev;
#if BT_WINDOW_LOG
        {
          /* 1 dong moi cua so: nhan + trang thai + |a| + bo dem may trang thai.
           * s = by_shock, u = shock_upright. Dong trang thai 2 s bo sot cua so. */
          const fall_state_t *ws = har_app_fall_state();
          const char *wst = (ws->state == FALL_STATE_IDLE)  ? "IDLE"
                          : (ws->state == FALL_STATE_SHOCK) ? "SHOCK" : "FALLEN";
          char wmin[10], wmax[10], wline[112];
          UART_FmtF2(wmin, sizeof wmin, ev.a_min, 0);
          UART_FmtF2(wmax, sizeof wmax, ev.a_max, 0);
          snprintf(wline, sizeof wline,
                   "W %s | %s | |a|%s..%s | r=%d g=%d l=%d t=%d s=%d u=%d\r\n",
                   har_label_name(ev.label), wst, wmin, wmax,
                   ws->active_run, ws->guard, ws->laying, ws->timer,
                   ws->by_shock, ws->shock_upright);
          Bluetooth_Send(wline);
        }
#endif
        if (ev.fall) {
          n_fall_total++;
          Buzzer_Start();
          printf("!!! FALL DETECTED !!!\r\n");
        }
        if (ev.recovered) {
          Buzzer_Stop();
          printf("Recovered from fall\r\n");
        }      }
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLRCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_SET);

  /*Configure GPIO pins : PC1 PC2 PC3 PC4
                           PC5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PC9 PC10 PC11 PC12 */
  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static void MX_TIM6_Init(void)
{
  __HAL_RCC_TIM6_CLK_ENABLE();

  TIM6->PSC = 9000 - 1;       // Prescaler: 90MHz / 9000 = 10kHz (0.1ms tick)
  TIM6->ARR = 200 - 1;        // Period: 10kHz / 200 = 50Hz (20ms)
  TIM6->DIER |= TIM_DIER_UIE; // Enable Update Interrupt
  TIM6->CR1 |= TIM_CR1_CEN;   // Enable Counter

  HAL_NVIC_SetPriority(TIM6_DAC_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(TIM6_DAC_IRQn);
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
