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
#include "ssd1306_conf.h"   // OLED display configuration (must be included first)
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <string.h>
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

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * System state machine enum
 * WAIT_FOR_MOTION: Wait for motion sensor trigger
 * WAIT_FOR_CODE: Wait for 4-digit keypad entry (30 seconds timeout)
 * WAIT_FOR_GESTURE: Wait for gesture recognition via UART (10 seconds timeout)
 * SERVO_ACTION: Execute servo action (door opening/closing)
 * SYSTEM_RESET: System reset state
 */
typedef enum
{
  WAIT_FOR_MOTION,
  WAIT_FOR_CODE,
  WAIT_FOR_GESTURE,
  SERVO_ACTION,
  SYSTEM_RESET
} SystemState;

SystemState current_state = WAIT_FOR_MOTION;

// Volatile flag set by motion sensor interrupt
volatile uint8_t motion_detected = 0;

/**
 * Display text on OLED screen (2 lines)
 * @param line1 - First line text
 * @param line2 - Second line text (NULL if not used)
 */
void DisplayText(const char* line1, const char* line2) {
    ssd1306_Fill(Black);              // Clear buffer (black background)
    ssd1306_SetCursor(5, 20);         // Set cursor at line 1
    ssd1306_WriteString((char*)line1, Font_7x10, White);
    if (line2 != NULL) {
         ssd1306_SetCursor(5, 34);     // Set cursor slightly higher for line 2
        ssd1306_WriteString((char*)line2, Font_6x8, White);
    }
    ssd1306_UpdateScreen();           // Refresh OLED screen via I2C
}

/**
 * Lights and buzzer blink helper
 * @param times - Number of blink cycles
 */
void BlinkLightsAndBuzzer(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(LED_Z_GPIO_Port, LED_Z_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_Z_GPIO_Port, LED_Z_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
    }
}

/**
 * Blink red LED and buzzer helper for failure cases
 */
void BlinkRedAndBuzzer(uint8_t times) {
    for (uint8_t i = 0; i < times; i++) {
        HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(Buzzer_GPIO_Port, Buzzer_Pin, GPIO_PIN_RESET);
        HAL_Delay(200);
    }
}

// Servo pulse width limits (in timer ticks / microseconds depending on timer config)
#define SERVO_MIN_PULSE 1000
#define SERVO_MAX_PULSE 2000

/**
 * Convert angle (0-180) to PWM pulse width
 */
static uint16_t AngleToPulse(uint8_t angle)
{
  if (angle > 180) angle = 180;
  return (uint16_t)(SERVO_MIN_PULSE + ((uint32_t)(SERVO_MAX_PULSE - SERVO_MIN_PULSE) * angle) / 180);
}

/**
 * 4x4 Keypad matrix layout
 * Rows: w1-w4 (GPIO outputs)
 * Columns: k1-k4 (GPIO inputs with pull-up)
 */
const char keypad_map[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

/**
 * Read single keypad key using matrix scanning
 * @return ASCII character of pressed key, or '\0' if no key pressed
 */
char ReadKeypad(void)
{
    HAL_GPIO_WritePin(w1_GPIO_Port, w1_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(w2_GPIO_Port, w2_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(w3_GPIO_Port, w3_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(w4_GPIO_Port, w4_Pin, GPIO_PIN_SET);

    for (int row = 0; row < 4; row++) {
        switch (row) {
            case 0: HAL_GPIO_WritePin(w1_GPIO_Port, w1_Pin, GPIO_PIN_RESET); break;
            case 1: HAL_GPIO_WritePin(w2_GPIO_Port, w2_Pin, GPIO_PIN_RESET); break;
            case 2: HAL_GPIO_WritePin(w3_GPIO_Port, w3_Pin, GPIO_PIN_RESET); break;
            case 3: HAL_GPIO_WritePin(w4_GPIO_Port, w4_Pin, GPIO_PIN_RESET); break;
        }

        if (HAL_GPIO_ReadPin(k1_GPIO_Port, k1_Pin) == GPIO_PIN_RESET) {
            return keypad_map[row][0];
        }
        if (HAL_GPIO_ReadPin(k2_GPIO_Port, k2_Pin) == GPIO_PIN_RESET) {
            return keypad_map[row][1];
        }
        if (HAL_GPIO_ReadPin(k3_GPIO_Port, k3_Pin) == GPIO_PIN_RESET) {
            return keypad_map[row][2];
        }
        if (HAL_GPIO_ReadPin(k4_GPIO_Port, k4_Pin) == GPIO_PIN_RESET) {
            return keypad_map[row][3];
        }

        switch (row) {
            case 0: HAL_GPIO_WritePin(w1_GPIO_Port, w1_Pin, GPIO_PIN_SET); break;
            case 1: HAL_GPIO_WritePin(w2_GPIO_Port, w2_Pin, GPIO_PIN_SET); break;
            case 2: HAL_GPIO_WritePin(w3_GPIO_Port, w3_Pin, GPIO_PIN_SET); break;
            case 3: HAL_GPIO_WritePin(w4_GPIO_Port, w4_Pin, GPIO_PIN_SET); break;
        }
    }
    return '\0';
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
  MX_USART2_UART_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  /* USER CODE BEGIN 2 */
  ssd1306_Init();
  HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3); // Start PWM signal generation for servo
  DisplayText("System ready", "Waiting for motion...");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
     switch (current_state) {

      case WAIT_FOR_MOTION:
          if (motion_detected == 1) {
              motion_detected = 0; // reset motion flag
              BlinkLightsAndBuzzer(3);
              DisplayText("Motion detected!", "Enter code...");
              current_state = WAIT_FOR_CODE;
          }
          break;

      case WAIT_FOR_CODE:
      {
        uint32_t start_time = HAL_GetTick();
        uint8_t code_entered = 0;
        char entered_code[5] = {0};
        int char_index = 0;
        char last_key = '\0';
        char screen_buffer[20];

        DisplayText("Enter code", "_ _ _ _");

        while ((HAL_GetTick() - start_time) < 30000) {
          char key = ReadKeypad();

          if (key != '\0' && last_key != key) {
            HAL_Delay(30); // simple debounce
            if (ReadKeypad() == key) {
              last_key = key;

              if (key == '*') {
                char_index = 0;
                memset(entered_code, 0, sizeof(entered_code));
                DisplayText("Code reset", "_ _ _ _");
                HAL_Delay(300);
                DisplayText("Enter code", "_ _ _ _");
                continue;
              }

              if (char_index < 4) {
                entered_code[char_index++] = key;
                entered_code[char_index] = '\0';

                sprintf(screen_buffer, "%c %c %c %c",
                        (entered_code[0] != '\0') ? entered_code[0] : '_',
                        (entered_code[1] != '\0') ? entered_code[1] : '_',
                        (entered_code[2] != '\0') ? entered_code[2] : '_',
                        (entered_code[3] != '\0') ? entered_code[3] : '_');
                DisplayText("Enter code", screen_buffer);

                if (char_index == 4) {
                  if (strcmp(entered_code, "3434") == 0) {
                    BlinkLightsAndBuzzer(3);
                    DisplayText("Code correct!", "Waiting gesture...");
                    current_state = WAIT_FOR_GESTURE;
                    code_entered = 1;
                    break;
                  } else {
                    DisplayText("Wrong code!", "Try again");
                    BlinkRedAndBuzzer(3);
                    HAL_Delay(2000);
                    char_index = 0;
                    memset(entered_code, 0, sizeof(entered_code));
                    DisplayText("Enter code", "_ _ _ _");
                    last_key = '\0';
                  }
                }
              }
            }
          }

          if (code_entered) {
            break;
          }
        }

        if (!code_entered) {
          DisplayText("Timeout", "Try again");
          BlinkRedAndBuzzer(3);
          DisplayText("System ready", "No motion...");
          current_state = WAIT_FOR_MOTION;
        }
      }
      break;

case WAIT_FOR_GESTURE:
      {
          uint8_t uart_buffer[1];
          uint32_t gesture_start = HAL_GetTick();
          DisplayText("Waiting gesture", "Send 'G'");
          uint8_t gesture_received = 0;

          // Flush UART buffer
          // Read and ignore any stale bytes that arrived before gesture waiting started
          uint8_t dummy;
          while (HAL_UART_Receive(&huart2, &dummy, 1, 0) == HAL_OK) {
              // Empty loop - consume bytes until RXNE is cleared
          }

          // Main loop waiting for gesture (10000 ms = 10 seconds)
          while ((HAL_GetTick() - gesture_start) < 10000) {
              if (HAL_UART_Receive(&huart2, uart_buffer, 1, 100) == HAL_OK) {
                  if (uart_buffer[0] == 'G') {
                      BlinkLightsAndBuzzer(3);
                      DisplayText("Gesture detected!", "Performing action...");
                      current_state = SERVO_ACTION;
                      gesture_received = 1;
                      break;
                  }
              }
          }

          if (!gesture_received) {
              DisplayText("No gesture", "Resetting...");
              BlinkRedAndBuzzer(3);
              DisplayText("System ready", "No motion...");
              current_state = WAIT_FOR_MOTION;
          }
      }
      break;

      case SERVO_ACTION:
      {
          // Sweep servo from 0 to 180 degrees
          for (int angle = 0; angle <= 180; angle += 10) {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, AngleToPulse(angle));
            HAL_Delay(150);
          }
          // Hold open for 10 seconds
          HAL_Delay(10000);
          // Sweep back from 180 to 0 degrees
          for (int angle = 180; angle >= 0; angle -= 10) {
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_3, AngleToPulse(angle));
            HAL_Delay(150);
          }
          DisplayText("Action complete", "Resetting...");
          HAL_Delay(2000);
          DisplayText("System ready", "No motion...");
          current_state = WAIT_FOR_MOTION;
      }
      break;

      case SYSTEM_RESET:
      {
          DisplayText("System reset", "Please wait...");
          HAL_Delay(2000);
          DisplayText("System ready", "No motion...");
          current_state = WAIT_FOR_MOTION;
      }
      break;
    }
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
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
  hi2c1.Init.ClockSpeed = 100000;
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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 19999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 1500;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|LED_Z_Pin|Buzzer_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, w4_Pin|w1_Pin|w2_Pin|w3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_C_GPIO_Port, LED_C_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin LED_Z_Pin Buzzer_Pin */
  GPIO_InitStruct.Pin = LD2_Pin|LED_Z_Pin|Buzzer_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : w4_Pin w1_Pin w2_Pin w3_Pin */
  GPIO_InitStruct.Pin = w4_Pin|w1_Pin|w2_Pin|w3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pins : k1_Pin k2_Pin */
  GPIO_InitStruct.Pin = k1_Pin|k2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_C_Pin */
  GPIO_InitStruct.Pin = LED_C_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_C_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : k3_Pin k4_Pin */
  GPIO_InitStruct.Pin = k3_Pin|k4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : Motion_sensor_Pin */
  GPIO_InitStruct.Pin = Motion_sensor_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(Motion_sensor_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  // Check if the interrupt comes from the motion sensor
  if (GPIO_Pin == Motion_sensor_Pin)
  {
    // Set motion detected flag
    motion_detected = 1;
  }
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
