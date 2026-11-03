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
#include "arm_math.h"
#include "icm20948.h"
#include "har_dsp.h"
#include "har_model.h"
#include "fall_detect.h"
#include "buzzer.h"
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// 1 = còi kêu ngay khi cấp nguồn để thử dây (nhấn KEY1 để tắt). Thử xong đổi về 0.
#define BUZZER_TEST_ON_BOOT  1

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
ICM20948_int16_vector3 mag_data;

static float features_buf[DSP_N_FEATURES];
static AI_Prediction_t ai_pred;

static float win_a_min = 100.0f;
static float win_a_max = 0.0f;
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
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
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
  Buzzer_Init();
#if BUZZER_TEST_ON_BOOT
  Buzzer_Start();
#endif
  DSP_Init();
  AI_Init();
  Fall_Init(NULL);

  uint8_t i2c_ok = 0;
  if (ICM20948_Init(&hi2c1, ICM20948_SDO_LOW) == HAL_OK) {
      ICM20948_Wake();
      ICM20948_WriteRegister(&ICM20948_REG_ACCEL_CONFIG_1, ICM20948_ACCEL_FCHOICE | ICM20948_ACCEL_FS_SEL_4g);
      ICM20948_WriteRegister(&ICM20948_REG_GYRO_CONFIG_1, ICM20948_GYRO_FCHOICE | ICM20948_GYRO_FS_SEL_1000);
      i2c_ok = 1;
  } else {
      // Bật PC4 (Kéo LOW) nếu khởi tạo I2C thất bại
      HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
  }
  AK09916_Init(AK09916_CNTL2_CONT_MEASURE_2);

  MX_TIM6_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    Buzzer_Update();

    // KEY1 (PC9, nhấn = 0): tắt còi hú khi đang báo té ngã
    static uint8_t key1_prev = 1;
    uint8_t key1 = (uint8_t)HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_9);
    if (key1 == 0 && key1_prev == 1) {
      Buzzer_Stop();
    }
    key1_prev = key1;

    static uint32_t last_uart_time = 0;
    static float cur_ax = 0.0f, cur_ay = 0.0f, cur_az = 0.0f;
    if (HAL_GetTick() - last_uart_time >= 2000) {
      last_uart_time = HAL_GetTick();
      int ax_i = (int)cur_ax, ax_f = (int)fabsf((cur_ax - (float)ax_i) * 100.0f);
      int ay_i = (int)cur_ay, ay_f = (int)fabsf((cur_ay - (float)ay_i) * 100.0f);
      int az_i = (int)cur_az, az_f = (int)fabsf((cur_az - (float)az_i) * 100.0f);
      printf("Hanh dong: %s | Trang thai nguy hiem: %s | Accel: X=%s%d.%02d, Y=%s%d.%02d, Z=%s%d.%02d\r\n",
             ai_pred.class_name ? ai_pred.class_name : "N/A", Fall_GetStateName(),
             (cur_ax < 0.0f && ax_i == 0) ? "-" : "", ax_i, ax_f,
             (cur_ay < 0.0f && ay_i == 0) ? "-" : "", ay_i, ay_f,
             (cur_az < 0.0f && az_i == 0) ? "-" : "", az_i, az_f);
    }

    if (flag_sample_imu) {
      flag_sample_imu = 0;
      
      // Nếu I2C ok thì mới đọc
      if (i2c_ok) {
          ICM20948_ReadAccelGryoRegisters(&accel_data, &gyro_data);
      } else {
          // Fake data để test nếu I2C hỏng
          accel_data.x = 0; accel_data.y = 0; accel_data.z = 8192; 
          gyro_data.x = 0; gyro_data.y = 0; gyro_data.z = 0;
      }

      IMU_Sample_t sample;
      sample.ax = (float)accel_data.x / 8192.0f;
      sample.ay = (float)accel_data.y / 8192.0f;
      sample.az = (float)accel_data.z / 8192.0f;
      cur_ax = sample.ax;
      cur_ay = sample.ay;
      cur_az = sample.az;
      sample.gx = ((float)gyro_data.x / 32.768f) * (3.14159265f / 180.0f); // Đổi sang rad/s với FS=1000dps
      sample.gy = ((float)gyro_data.y / 32.768f) * (3.14159265f / 180.0f);
      sample.gz = ((float)gyro_data.z / 32.768f) * (3.14159265f / 180.0f);

      float mag = sqrtf(sample.ax * sample.ax + sample.ay * sample.ay + sample.az * sample.az);
      if (mag < win_a_min) win_a_min = mag;
      if (mag > win_a_max) win_a_max = mag;

      bool window_ready = DSP_PushSample(&sample);

      if (window_ready) {
        DSP_ExtractFeatures(features_buf);
        AI_Predict(features_buf, &ai_pred);

        FallEvent_t event = Fall_Update(ai_pred.predicted_class, win_a_min, win_a_max);

        win_a_min = 100.0f;
        win_a_max = 0.0f;

        // Còi chỉ kêu khi xác nhận té ngã, tự tắt khi người dùng đứng dậy
        if (event == FALL_EVENT_DETECTED) {
          Buzzer_Start();
        } else if (event == FALL_EVENT_RECOVERED) {
          Buzzer_Stop();
        }

        FallState_t state = Fall_GetState();
        if (state == FALL_STATE_IDLE) {
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_SET);
        } else if (state == FALL_STATE_SHOCK) {
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1 | GPIO_PIN_2, GPIO_PIN_RESET);
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3, GPIO_PIN_SET);
        } else if (state == FALL_STATE_FALLEN) {
          HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3, GPIO_PIN_RESET);
        }
      }
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
