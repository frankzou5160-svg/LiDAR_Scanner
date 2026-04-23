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
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>
#include <string.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */

  // Init display
  ssd1306_Init();

  // Clear screen
  ssd1306_Fill(Black);

  ssd1306_SetCursor(0,0);
  ssd1306_WriteString("SCANNER", Font_11x18, White);

  ssd1306_SetCursor(0,24);
  ssd1306_WriteString("READY...", Font_11x18, White);

  // Update Screen
  ssd1306_UpdateScreen();
  HAL_Delay(2000);
  ssd1306_Fill(Black);

  const uint8_t stepSeq[8][4]= {
		  {1,0,0,0},
		  {1,1,0,0},
		  {0,1,0,0},
		  {0,1,1,0},
		  {0,0,1,0},
		  {0,0,1,1},
		  {0,0,0,1},
		  {1,0,0,1}
  };

  int StepIndex = 0;		//current position in step seq
  int StepCount = 0;		//total steps taken this sweep
  float angle = 0.0f;		//current angle in degrees
  uint32_t lastStepTime = 0;  // timestamp of last motor step
  const uint32_t STEP_INTERVAL = 3;        // ms between steps
  const int STEPS_PER_REV     = 14155;      // 28BYJ-48 full revolution is 4096 (11796 is calculated after gear ratio is considered

  // Rolling average buffer
  uint16_t distBuf[5] = {0};
  uint8_t  bufIdx = 0;
  uint8_t  bufFull = 0;
  static uint16_t lastValidDist = 0;
  static uint8_t hasValidReading = 0;
  uint16_t avgDist = 0;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	  uint32_t now = HAL_GetTick();
	  int IsValid = 0;
	  int DidStep = 0;
	  /* Motor Control Start */
	  if( now - lastStepTime >= STEP_INTERVAL ){
		  lastStepTime = now;

		  // Current Step Seq
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0, stepSeq[StepIndex][0]);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, stepSeq[StepIndex][1]);
		  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, stepSeq[StepIndex][2]);
		  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, stepSeq[StepIndex][3]);

		  //Advancing Step Index
		  StepIndex = (StepIndex + 1 ) % 8;
		  StepCount ++;

		  //Calc Angle
		  angle = (StepCount % STEPS_PER_REV) * 360.0f / STEPS_PER_REV;
		  DidStep = 1;

//		  static uint8_t sensorSkip = 0;
//		  sensorSkip++;
//		  if (sensorSkip < 4) goto transmit;  // only read sensor every 4th step
//		  sensorSkip = 0;

		  //check sweep complete
		  if (StepCount % STEPS_PER_REV == 0){
		     HAL_UART_Transmit(&huart2, (uint8_t*)"A360\r\n", 6, 10);
		  }
	  }
	  if (!DidStep) continue;
	  if (StepCount % 8 != 0) goto skip;
	  /* Motor Control End */

	  /* Sensor READ Start */
	  //flush stale bytes
	  if (StepCount % 8 == 0){

	  uint8_t flush;
	  while (HAL_UART_Receive(&huart1, &flush, 1, 0) == HAL_OK) {} // drain buffer

	  uint8_t byte;

	  // hunt for first 0xx59
	  uint8_t found = 0;
	  for (int attempt =0; attempt < 5; attempt ++){
		  if (HAL_UART_Receive(&huart1,&byte,1,2) == HAL_OK && byte ==0x59){
			  found = 1;
			  break;
		  }

	  }

	  if (!found) goto transmit;

	  // Second 0x59
	  if (HAL_UART_Receive(&huart1, &byte, 1, 2) != HAL_OK) goto transmit;
	  if (byte != 0x59) goto transmit;

	  uint8_t frame[9];
	  frame[0]=0x59;
	  frame[1]=0x59;

	  if (HAL_UART_Receive(&huart1,&frame[2],7,5) != HAL_OK) goto transmit;

	  //checksum
	  uint8_t checksum = 0;
	  for (int i = 0; i < 8; i++) checksum += frame[i];
	  if (checksum != frame[8]) goto transmit;

	  uint16_t dst_cm = frame[2] + (frame[3] << 8);
	  uint16_t strength = frame[4] + (frame[5] << 8);


	  //valid signal check
	  if(strength < 100 || strength >= 65535 || dst_cm >= 65535) goto transmit;


	  //Rolling avg
	  distBuf[bufIdx] = dst_cm;
	  bufIdx = (bufIdx + 1) % 5;
	  if (bufIdx == 0) bufFull = 1;

	  uint32_t sum = 0;
	  uint8_t count = bufFull ? 5 : bufIdx;
	  for (int i = 0; i < count; i++) sum += distBuf[i];
	  avgDist = sum / count;

	  //turn on valid signal
	  IsValid = 1;

transmit:
      char msg[32];
	  if (IsValid){
		  sprintf(msg, "A%03d,%d\r\n", (int)angle, avgDist);
		  lastValidDist = avgDist;
		  hasValidReading = 1;
	  }
	  else {
		  sprintf(msg, "A%03d,0\r\n", (int)angle);
		  hasValidReading = 0;
	  }
	  HAL_UART_Transmit(&huart2, (uint8_t*)msg, strlen(msg), 10);




	  //Update OLED
	  static uint8_t oledCount = 0;
	  oledCount++;
	  if (oledCount >= 20){
		  oledCount = 0;
		  char line1[16], line2[16];

		  ssd1306_Fill(Black);
		  ssd1306_SetCursor(0,0);
		  ssd1306_WriteString ("SCANNING",Font_7x10, White);
		  ssd1306_SetCursor(0,24);
		  sprintf(line1, "Ang: %3d",(int)angle);
		  ssd1306_WriteString (line1,Font_7x10, White);
		  if (hasValidReading){
			  ssd1306_SetCursor(0,38);
			  sprintf(line2, "Dst: %4dcm",lastValidDist);
			  ssd1306_WriteString (line2,Font_7x10, White);
		  }
		  else {
			  ssd1306_SetCursor(0,38);
			  ssd1306_WriteString ("No TARGET",Font_7x10, White);
		  }
		  ssd1306_UpdateScreen();
	  }


	  /* Sensor READ End*/
	  }
skip:;
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
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
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : PA0 PA1 PA4 LD2_Pin */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1|GPIO_PIN_4|LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : PB0 */
  GPIO_InitStruct.Pin = GPIO_PIN_0;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
