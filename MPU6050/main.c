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
#include "i2c.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "MPU6050.h"
#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

float roll = 0, pitch = 0, yaw = 0;
float n = 0.99f;
float gz_bias = 0;

int still_cnt = 0;
int i = 0;
double r_arr[5] = {0};
double p_arr[5] = {0};
double y_arr[5] = {0};
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
uint8_t mpu_data[14] = {0};
uint8_t startaddress = 0x3b;
double ax = 0.0,ay =  0.0,az = 0.0;
double gx = 0.0, gy = 0.0,gz = 0.0;
mpu6050_init();
		double gz_sum = 0;
		uint8_t cal_data[14];
		for(int cal_i = 0; cal_i < 200; cal_i++)
		{
			HAL_I2C_Mem_Read(&hi2c1, 0xd1, startaddress, I2C_MEMADD_SIZE_8BIT, cal_data, 14, 50);
			short int cal_gz = (cal_data[12] << 8) | cal_data[13];
			gz_sum += (double)cal_gz;
			HAL_Delay(5);
		}
		gz_sum /= 200.0;
		gz_bias = (float)(gz_sum / 65.5);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
		HAL_I2C_Mem_Read(&hi2c1,0xd1,startaddress,I2C_MEMADD_SIZE_8BIT,mpu_data,14,50);
		
		float temp = (mpu_data[6]<<8)|mpu_data[7];
		if(temp > 32768)
		{
			temp -= 65536;
		}
		temp = (36.53 + temp/340);
		
		short int ax1 = ((mpu_data[0]<<8)|mpu_data[1]);ax = (double)ax1/16384;
		short int ay1 = ((mpu_data[2]<<8)|mpu_data[3]);ay = (double)ay1/16384;
		short int az1 = ((mpu_data[4]<<8)|mpu_data[5]);az = (double)az1/16384;
		
		short int gx1 = ((mpu_data[8]<<8)|mpu_data[9]);gx = (double)gx1/65.5;
		short int gy1 = ((mpu_data[10]<<8)|mpu_data[11]);gy = (double)gy1/65.5;
		short int gz1 = ((mpu_data[12]<<8)|mpu_data[13]);gz = (double)gz1/65.5;
		
		float gz_cal = (float)gz - gz_bias; 
    
    if(fabs(gz_cal) < 0.3f)                   
    {
        still_cnt++;
        if(still_cnt > 100)     
        {
            gz_bias = 0.999f * gz_bias + 0.001f * (float)gz;
        }
    }
    else
    {
        still_cnt = 0;
    }
		
		float acc_roll  = atan2(ay, az) * 57.3f;
    float acc_pitch = atan2(-ax, sqrt(ay*ay + az*az)) * 57.3f;
    
    roll  = n * (roll  + gx * 0.008f) + (1 - n) * acc_roll;
    pitch = n * (pitch + gy * 0.008f) + (1 - n) * acc_pitch;
    yaw   = yaw + (gz - gz_bias) * 0.008f;
		
		if(i >= 5)
		{
			i = 0;
			
			for(int j = 0;j < 5-1; j++)
     {
         for(int k = 0;k< 5-j-1;k++)
         {
             if (y_arr[k]>y_arr[k+1])
             {
                 double temp = y_arr[k];
                 y_arr[k]=y_arr[k+1];
                 y_arr[k+1]=temp;
             }

         }
			 }
			
			roll = (double)((double)(r_arr[1]+r_arr[2]+r_arr[3]))/(double)3.0000;
			pitch = (double)((double)(p_arr[1]+p_arr[2]+p_arr[3]))/(double)3.0000;
			yaw		= (double)((double)(y_arr[1]+y_arr[2]+y_arr[3]))/(double)3.0000;
			printf("%.2f,%.2f,%.2f\n",roll,pitch,yaw);
		}
		else
		{
			r_arr[i] = roll;
			p_arr[i] = pitch;
			y_arr[i] = yaw;
			i++;
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
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
