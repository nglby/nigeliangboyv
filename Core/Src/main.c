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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bmi088.h"
#include <math.h>
#include "pid.h"
#include "control.h"
#include "ms53l0.h"
#include "guangliu.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* 当前姿态控制尚未使用光流数据。逐字节UART中断会高频抢占控制环，
 * 姿态调通前先关闭；以后接入位置环时应改为DMA循环接收后再置1。 */
#define ENABLE_OPTICAL_FLOW  0

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
volatile float    tof_height_mm = 0.0f;     // 滤波后的TOF高度，供TIM3中断读取
volatile uint8_t  tof_height_valid = 0;     // 1=有有效高度
volatile uint16_t tof_raw_mm = 0;           // TOF原始读数
volatile uint32_t tof_read_cnt = 0;         // 读取计数
volatile uint32_t tof_timeout_cnt = 0;      // 超时计数
/* 高度PID */
PID_t pid_height;               // 高度PID控制器
volatile float    height_target_mm = 500.0f; // 目标高度
volatile uint8_t  alt_hold_enabled = 0;      // 定高开关，0=手动油门 1=定高

/* 高度低通滤波状态 */
float             height_lpf = 0.0f;        // 低通滤波值

BMI088_Data_t imu;
float pitch = 0, roll = 0, yaw = 0;
float gx_off = 0, gy_off = 0, gz_off = 0;
PID_t pid_pitch, pid_roll;             /* 外环: 角度PID */
PID_t pid_pitch_rate, pid_roll_rate;   /* 内环: 角速度PID */
filter_parameter accel_lpf_param, gyro_lpf_param;
filter_buffer accel_lpf_buf[3], gyro_lpf_buf[3];
Mixer_t mixer;
Control_t flight_control;
volatile float pitch_target_cmd = 0.0f;
volatile float roll_target_cmd = 0.0f;
uint32_t imu_ok = 0, imu_fail = 0;
volatile uint32_t control_last_cycles = 0;
volatile uint32_t control_max_cycles = 0;
volatile uint32_t control_overrun = 0;
volatile uint8_t mixer_saturated = 0;
volatile uint16_t throttle_base_debug = 1000;
volatile uint8_t startup_phase_debug = 1;
uint8_t debug_mode = 0;  /* 1=调试模式(电机停转,只打印传感器), 0=正常飞行 */

#define CONTROL_DT        0.005f   /* 与官方一致: TIM3 = 200Hz, dt固定5ms */
#define ATTITUDE_ALPHA    0.96f    /* 约0.12s时间常数；不可沿用500Hz下的0.98 */
#define ANGLE_KP_NEAR     5.0f     /* 平衡点附近防震 */
#define ANGLE_KP_FAR      11.0f    /* 稍增大倾角回正力，仍保持12度的平滑过渡区 */
#define ANGLE_GAIN_START  1.0f     /* 从1度开始增加P增益 */
#define ANGLE_GAIN_FULL   12.0f    /* 延长增益过渡区，12度以上才使用最大P增益 */
#define RATE_I_SEP_ERR    30.0f    /* 大角速度误差时不继续累积I，只允许反向卸载 */
#define RAMP_START        1000.0f  /* 斜坡起点: ESC idle, 电机不转 */
#define THROTTLE_TARGET   1300.0f  /* 提高悬停基础PWM；姿态修正会在此基础上产生差速 */
#define RAMP_TICKS        1600     /* 8秒内四路同步从1000线性增加到THROTTLE_TARGET */
#define STABILIZE_TICKS   200      /* 到达目标油门后保持1秒启动阶段 */
uint16_t startup_tick = 0;        /* 启动阶段计数 */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* 姿态角增益调度：零点附近使用官方基础增益，避免重新引入震荡；
 * 倾角增大时平滑提高P增益，避免依赖I项慢慢累积后才开始回正。 */
static float Attitude_Update_Outer(PID_t *pid, float target, float angle, float dt)
{
    float blend = (FastAbs(target - angle) - ANGLE_GAIN_START)
                / (ANGLE_GAIN_FULL - ANGLE_GAIN_START);
    blend = constrain_float(blend, 0.0f, 1.0f);
    pid->kp = ANGLE_KP_NEAR + (ANGLE_KP_FAR - ANGLE_KP_NEAR) * blend;
    return PID_Update_Outer(pid, target, angle, dt);
}

/* 低通滤波器不能从全0状态启动，否则加速度/姿态会额外花一段时间收敛。 */
static void Filter_Seed(filter_buffer *buf, float value)
{
    uint8_t i;
    for (i = 0; i < 3; i++) {
        buf->input[i] = value;
        buf->output[i] = value;
    }
}

/* 整体平移油门后仍可完整保留差速；只有姿态请求跨度超过可用PWM范围，
 * 才属于真正的执行器饱和并触发抗积分。 */
static uint8_t Mixer_Would_Saturate(const Mixer_t *m, float lower_limit)
{
    float raw1 = m->throttle - m->pitch_out + m->roll_out - m->yaw_out;
    float raw2 = m->throttle - m->pitch_out - m->roll_out + m->yaw_out;
    float raw3 = m->throttle + m->pitch_out - m->roll_out + m->yaw_out;
    float raw4 = m->throttle + m->pitch_out + m->roll_out - m->yaw_out;
    float raw_min = raw1;
    float raw_max = raw1;
    if (raw2 < raw_min) raw_min = raw2;
    if (raw3 < raw_min) raw_min = raw3;
    if (raw4 < raw_min) raw_min = raw4;
    if (raw2 > raw_max) raw_max = raw2;
    if (raw3 > raw_max) raw_max = raw3;
    if (raw4 > raw_max) raw_max = raw4;
    return (raw_max - raw_min) > (MOTOR_MAX - lower_limit);
}

/* 启动斜坡专用混控。允许PWM低于正常飞行MOTOR_MIN，但仍保留姿态差速，
 * 避免电机已经起转时四路输出却完全相同。 */
static void Startup_Mixer_Output(Mixer_t *m)
{
    float lower_limit = (m->throttle < MOTOR_MIN) ? RAMP_START : MOTOR_MIN;
    Mixer_Update_Limited(m, lower_limit);

    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint16_t)m->m1);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (uint16_t)m->m2);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint16_t)m->m3);
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (uint16_t)m->m4);
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
  /* SystemClock_Config会重设SysTick优先级；必须在其后设为最高，
   * 才能让TIM3中断内HAL_I2C的2ms超时计数继续走。 */
  HAL_NVIC_SetPriority(SysTick_IRQn, 0, 0);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_TIM2_Init();
  MX_I2C1_Init();
  MX_I2C2_Init();
  MX_USART2_UART_Init();
  MX_TIM3_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */
	MS53L0_Init();
	printf("MS53L0 init done\r\n");
	#if ENABLE_OPTICAL_FLOW
	OpticalFlow_Init();
  printf("Optical Flow ready\r\n");
	#else
  HAL_NVIC_DisableIRQ(USART2_IRQn);
  printf("Optical Flow RX disabled during attitude tuning\r\n");
	#endif

	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 990);
	__HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 990);
	
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 990);
	__HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 990);
	HAL_Delay(5000);
	printf("ESC unlock done\r\n");

  uint8_t st = BMI088_Init(&hi2c1);
  if (st == 0)
      printf("BMI088 init OK\r\n");
  else {
      printf("BMI088 init FAIL: %d\r\n", st);
      while (1);
  }

  printf("Gyro calibrating... keep still\r\n");
  HAL_Delay(1000);  /* 给机体和操作人员留出静置时间 */
  for (int i = 0; i < 512; i++)
	{
      BMI088_ReadAll(&hi2c1, &imu);
      gx_off += imu.gx;
      gy_off += imu.gy;
      gz_off += imu.gz;
      HAL_Delay(5); /* 与BMI088的200Hz ODR同步，避免重复统计同一帧 */
  }
  gx_off /= 512.0f;
  gy_off /= 512.0f;
  gz_off /= 512.0f;
  printf("Gyro offset: %.2f, %.2f, %.2f\r\n", gx_off, gy_off, gz_off);
  #define SAMPLE_FREQ   200.0f
  #define D_CUTOFF_FREQ 30.0f
  #define IMU_CUTOFF_FREQ 30.0f
  /* 官方也会先对陀螺仪和加速度计做30Hz低通；缺少这一层时，
   * 机架振动会直接进入P/D项，最容易在平衡点附近来回震荡。 */
  set_cutoff_frequency(SAMPLE_FREQ, IMU_CUTOFF_FREQ, &accel_lpf_param);
  set_cutoff_frequency(SAMPLE_FREQ, IMU_CUTOFF_FREQ, &gyro_lpf_param);

  /* 用当前重力方向直接初始化姿态和滤波器。旧代码固定从0度开始，
   * 当开机时机体已有倾角，互补滤波需要约0.5秒才显示真实角度。 */
  if (BMI088_ReadAll(&hi2c1, &imu) == HAL_OK) {
    imu.gx -= gx_off;
    imu.gy -= gy_off;
    imu.gz -= gz_off;
    Filter_Seed(&accel_lpf_buf[0], imu.ax);
    Filter_Seed(&accel_lpf_buf[1], imu.ay);
    Filter_Seed(&accel_lpf_buf[2], imu.az);
    Filter_Seed(&gyro_lpf_buf[0], imu.gx);
    Filter_Seed(&gyro_lpf_buf[1], imu.gy);
    Filter_Seed(&gyro_lpf_buf[2], imu.gz);
    pitch = atan2f(imu.ay, sqrtf(imu.ax * imu.ax + imu.az * imu.az)) * 57.2958f;
    roll  = atan2f(imu.ax, sqrtf(imu.ay * imu.ay + imu.az * imu.az)) * 57.2958f;
    printf("Attitude init: pitch=%.2f roll=%.2f\r\n", pitch, roll);
  }

  /* 保留官方角度环响应。I项需要足以消除四电机推力静差，
   * D项只提供必要阻尼，避免在接近平衡点时过度“刹车”。 */
  PID_Init_Outer(&pid_pitch,      5.0f, 0.0f, 0.0f, 30.0f, 100.0f, 500.0f);
  PID_Init_Outer(&pid_roll,       5.0f, 0.0f, 0.0f, 30.0f, 100.0f, 500.0f);
	PID_Init_Outer(&pid_height,
    0.4f,     /* kp: 每偏离1mm加0.4个PWM点，离目标100mm就是40点，很小很稳 */
    0.0f,     /* ki: 先不加积分，稳了再说 */
    0.0f,     /* kd: 先不加微分 */
    500.0f,   /* err_max: 偏差超过500mm限幅 */
    200.0f,   /* integ_max: 暂不用 */
    300.0f);  /* out_limit: 高度PID输出不超过±300 PWM点 */
  PID_Init_Inner(&pid_pitch_rate, 0.9f, 0.9f, 1.2f, 600.0f, 100.0f, 400.0f,
                 SAMPLE_FREQ, D_CUTOFF_FREQ);
  PID_Init_Inner(&pid_roll_rate,  0.9f, 0.9f, 1.2f, 600.0f, 100.0f, 400.0f,
                 SAMPLE_FREQ, D_CUTOFF_FREQ);
  pid_pitch_rate.integrate_separation_flag = 1;
  pid_pitch_rate.integrate_separation_err = RATE_I_SEP_ERR;
  pid_roll_rate.integrate_separation_flag = 1;
  pid_roll_rate.integrate_separation_err = RATE_I_SEP_ERR;
  Control_Init(&flight_control);
  Control_UART_Start();
  mixer.throttle  = MOTOR_MIN;
  mixer.pitch_out = 0;
  mixer.roll_out  = 0;
  mixer.yaw_out   = 0;
  mixer.m1 = 990.0f;
  mixer.m2 = 990.0f;
  mixer.m3 = 990.0f;
  mixer.m4 = 990.0f;

  if (debug_mode) {
    printf("DEBUG MODE: motors stopped, IMU in main loop\r\n");
  } else {
    printf("DISARMED: motors fixed at 990, send OK to start\r\n");
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  uint8_t control_loop_started = 0U;
  while (1)
  {
    uint32_t target_update_primask;

    Control_ProcessUART(&flight_control);
		static uint32_t tof_tick = 0;
		if (HAL_GetTick() - tof_tick > 50)
			{
				uint16_t raw = MS53L0_ReadMM();
				tof_read_cnt++;
				if (raw == 0xFFFF || raw < 30 || raw > 2000)
					{
						tof_timeout_cnt++;
						tof_height_valid = 0;
					} 
				else
					{
						tof_raw_mm = raw;
						
						/* 关键：倾斜修正。飞机的TOF是斜射到地面的，不是垂直距离 */
						float cos_pitch = cosf(pitch * 0.0174533f);  // 度转弧度
						float cos_roll  = cosf(roll  * 0.0174533f);
						float corrected_h = (float)raw * cos_pitch * cos_roll;
						
						/* 一阶低通，平滑TOF噪声 */
						if (height_lpf < 1.0f)
							{
								height_lpf = corrected_h;  // 首次直接赋值
							} 
						else
							{
								height_lpf = 0.8f * height_lpf + 0.2f * corrected_h;
							}
						tof_height_mm = height_lpf;
						tof_height_valid = 1;
				}
				tof_tick = HAL_GetTick();
			}
    if (!debug_mode && !control_loop_started && Control_IsArmed(&flight_control)) {
      /* 收到OK后才从零开始斜坡，避免串口中途连接导致复位后直接起转。 */
      startup_tick = 0U;
      pid_pitch_rate.integrate = 0.0f;
      pid_roll_rate.integrate = 0.0f;
      CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
      DWT->CYCCNT = 0;
      DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
      HAL_TIM_Base_Start_IT(&htim3);
      control_loop_started = 1U;
      printf("Control loop started (200Hz TIM3 interrupt)\r\n");
      printf("Throttle: 1000->%.0f in 8.0s, attitude PID ramps in with throttle\r\n",
             THROTTLE_TARGET);
      printf("RC USART1 IRQ: W/S/Q/E=%.1fdeg, Z=level, timeout=%lums, A/D disabled\r\n",
             CMD_TILT_ANGLE, (uint32_t)CMD_TIMEOUT_MS);
    }
    /* 两个目标必须作为一组更新，防止200Hz中断读到一新一旧。 */
    target_update_primask = __get_PRIMASK();
    __disable_irq();
    pitch_target_cmd = flight_control.pitch_target;
    roll_target_cmd = flight_control.roll_target;
    if (target_update_primask == 0U) {
      __enable_irq();
    }

    if (debug_mode) {
      if (BMI088_ReadAll(&hi2c1, &imu) == HAL_OK) {
        imu.gx -= gx_off;
        imu.gy -= gy_off;
        imu.gz -= gz_off;

        /* 与官方sensor_raw_update一致，在姿态融合和PID之前过滤IMU。 */
        imu.ax = butterworth(imu.ax, &accel_lpf_buf[0], &accel_lpf_param);
        imu.ay = butterworth(imu.ay, &accel_lpf_buf[1], &accel_lpf_param);
        imu.az = butterworth(imu.az, &accel_lpf_buf[2], &accel_lpf_param);
        imu.gx = butterworth(imu.gx, &gyro_lpf_buf[0], &gyro_lpf_param);
        imu.gy = butterworth(imu.gy, &gyro_lpf_buf[1], &gyro_lpf_param);
        imu.gz = butterworth(imu.gz, &gyro_lpf_buf[2], &gyro_lpf_param);
        float pitch_acc = atan2f(imu.ay, sqrtf(imu.ax*imu.ax + imu.az*imu.az)) * 57.2958f;
        float roll_acc  = atan2f(imu.ax, sqrtf(imu.ay*imu.ay + imu.az*imu.az)) * 57.2958f;
        pitch = 0.98f * (pitch + imu.gx * 0.01f) + 0.02f * pitch_acc;
        roll  = 0.98f * (roll - imu.gy * 0.01f) + 0.02f * roll_acc;
        /* 官方级联PID: 外环(角度P) → 内环(角速度PID+巴特沃斯D项) */
        float pdr = Attitude_Update_Outer(&pid_pitch, pitch_target_cmd, pitch, 0.01f);
        float rdr = Attitude_Update_Outer(&pid_roll,  roll_target_cmd,  roll,  0.01f);
        float po = PID_Update_Inner(&pid_pitch_rate, pdr, imu.gx,  0.01f);
        float ro = PID_Update_Inner(&pid_roll_rate,  rdr, -imu.gy, 0.01f);
        /* 调试模式绝不向ESC输出起转脉宽，只计算并显示控制量。 */
        mixer.pitch_out = po;
        mixer.roll_out  = ro;
        mixer.yaw_out   = 0;
        mixer.m1 = 990.0f;
        mixer.m2 = 990.0f;
        mixer.m3 = 990.0f;
        mixer.m4 = 990.0f;
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 990);
        __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 990);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 990);
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 990);
      }
    }

    static uint32_t dbg_tick = 0;
    if (HAL_GetTick() - dbg_tick > 500) {
      /* 4KB主栈下使用简短浮点输出，直接显示物理量。 */
      printf("IMU gx=%.2f gy=%.2f gz=%.2f pitch=%.2f roll=%.2f po=%.2f ro=%.2f pi=%.2f ri=%.2f\r\n",
        imu.gx, imu.gy, imu.gz, pitch, roll,
        mixer.pitch_out, mixer.roll_out,
        pid_pitch_rate.integrate, pid_roll_rate.integrate);
      printf("MOTOR base=%u phase=%u m1=%u m2=%u m3=%u m4=%u\r\n",
        throttle_base_debug, startup_phase_debug,
        (uint16_t)mixer.m1, (uint16_t)mixer.m2,
        (uint16_t)mixer.m3, (uint16_t)mixer.m4);
      printf("CTRL last=%luus max=%luus overrun=%lu | IMU ok=%lu fail=%lu | MIX sat=%u | RC fs=%lu err=%lu | FLOW bytes=%lu err=%lu\r\n",
        control_last_cycles / (SystemCoreClock / 1000000U),
        control_max_cycles / (SystemCoreClock / 1000000U),
        control_overrun, imu_ok, imu_fail, mixer_saturated,
        control_failsafe_count, control_uart_error_count,
        raw_byte_cnt, uart_err_cnt);
			printf("TOF: %umm valid=%u cnt=%lu timeout=%lu\r\n",
   tof_height_valid ? tof_raw_mm : 0,
   tof_height_valid, tof_read_cnt, tof_timeout_cnt);
      dbg_tick = HAL_GetTick();
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

/* USART1 使用寄存器方式读 SR/DR。返回 1 让向量入口跳过
 * HAL_UART_IRQHandler()，避免 HAL 再次处理已经读走的字节。
 * 函数放在 USER CODE 区，CubeMX 重新生成时不会覆盖。 */
uint8_t App_USART1_IRQHandler(void)
{
    Control_UART_IRQHandler();
    return 1U;
}

/* TIM3定时中断回调 - 官方同频200Hz控制循环
 * 控制循环在中断中运行, 不受主循环printf/TOF/光流影响, dt固定5ms
 * NVIC优先级: SysTick=0(最高) > TIM3=1 > USART2=2
 * SysTick优先级设为0确保HAL_I2C的HAL_GetTick超时机制在中断中正常工作 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    uint32_t control_start_cycles;
    uint32_t elapsed_cycles;

    if (htim->Instance != TIM3) return;
    control_start_cycles = DWT->CYCCNT;

    /* 三段启动:
     *   阶段1 (0~8s): 基础PWM线性增加，姿态PID输出同步从0平滑增加到100%，积分清零
     *   阶段2 (8~9s): 基础PWM保持THROTTLE_TARGET，姿态PID已全量控制
     *   阶段3 (9s+):  基础PWM保持THROTTLE_TARGET，姿态PID持续控制 */
    uint8_t phase1 = (startup_tick < RAMP_TICKS);
    uint8_t phase2 = (startup_tick < RAMP_TICKS + STABILIZE_TICKS);
    float fade = 1.0f;  /* 正常飞行阶段始终保持全效PID */
    if (startup_tick < RAMP_TICKS + STABILIZE_TICKS) {
        startup_tick++;
    }
    if (phase1) 
			{
        mixer.throttle = RAMP_START + (THROTTLE_TARGET - RAMP_START)
                       * ((float)startup_tick / RAMP_TICKS);
        /* 飞机可能在斜坡中途离地；PID不能等到8秒后才接管。
         * 与油门同速渐入可在离地时已有相应修正，又不会突然加差速。 */
        fade = (float)startup_tick / RAMP_TICKS;
			} 
		else if (phase2)
			{
        mixer.throttle = THROTTLE_TARGET;
        fade = 1.0f;
			}
		else 
			{     /* 定高闭环 */
				float height_throttle_delta = 0.0f;
				if (alt_hold_enabled && tof_height_valid)
					{
							height_throttle_delta = PID_Update_Outer(&pid_height,
							height_target_mm, tof_height_mm, CONTROL_DT);
							if (height_throttle_delta > 300.0f)  height_throttle_delta = 300.0f;
							if (height_throttle_delta < -300.0f) height_throttle_delta = -300.0f;
					}
      mixer.throttle = THROTTLE_TARGET + height_throttle_delta;
        if (mixer.throttle < MOTOR_MIN)  mixer.throttle = MOTOR_MIN;
        if (mixer.throttle > MOTOR_MAX)  mixer.throttle = MOTOR_MAX;
    }
    /* phase3: throttle 保持 THROTTLE_TARGET, fade 保持 1.0 */
    throttle_base_debug = (uint16_t)mixer.throttle;
    startup_phase_debug = phase1 ? 1U : (phase2 ? 2U : 3U);

    if (BMI088_ReadAll(&hi2c1, &imu) == HAL_OK)
    {
        imu_ok++;
        imu.gx -= gx_off;
        imu.gy -= gy_off;
        imu.gz -= gz_off;

        /* 与官方sensor_raw_update一致，在姿态融合和PID之前过滤IMU。 */
        imu.ax = butterworth(imu.ax, &accel_lpf_buf[0], &accel_lpf_param);
        imu.ay = butterworth(imu.ay, &accel_lpf_buf[1], &accel_lpf_param);
        imu.az = butterworth(imu.az, &accel_lpf_buf[2], &accel_lpf_param);
        imu.gx = butterworth(imu.gx, &gyro_lpf_buf[0], &gyro_lpf_param);
        imu.gy = butterworth(imu.gy, &gyro_lpf_buf[1], &gyro_lpf_param);
        imu.gz = butterworth(imu.gz, &gyro_lpf_buf[2], &gyro_lpf_param);

        float pitch_acc = atan2f(imu.ay, sqrtf(imu.ax*imu.ax + imu.az*imu.az)) * 57.2958f;
        float roll_acc  = atan2f(imu.ax, sqrtf(imu.ay*imu.ay + imu.az*imu.az)) * 57.2958f;
        /* 互补滤波始终运行, 保持姿态估计连续 */
        pitch = ATTITUDE_ALPHA * (pitch + imu.gx * CONTROL_DT)
              + (1.0f - ATTITUDE_ALPHA) * pitch_acc;
        roll  = ATTITUDE_ALPHA * (roll - imu.gy * CONTROL_DT)
              + (1.0f - ATTITUDE_ALPHA) * roll_acc;
        yaw  += imu.gz * CONTROL_DT;

        /* PID始终计算(预热), 输出量由fade控制 */
        /* 官方级联PID: 外环(角度P) → 内环(角速度PID+巴特沃斯D项) */
        float pitch_desired_rate = Attitude_Update_Outer(&pid_pitch, pitch_target_cmd,
                                                         pitch, CONTROL_DT);
        float roll_desired_rate  = Attitude_Update_Outer(&pid_roll, roll_target_cmd,
                                                         roll, CONTROL_DT);
        float pitch_i_before = pid_pitch_rate.integrate;
        float roll_i_before = pid_roll_rate.integrate;
        uint8_t integral_hold = phase1;
        float mixer_lower_limit = phase1 ? RAMP_START : MOTOR_MIN;

        /* 官方在油门尚未进入有效控制区时持续清姿态积分。
         * 斜坡阶段只使用立即响应的P/D，避免机体尚未起转时I项先攒满。 */
        if (integral_hold) {
            pid_pitch_rate.integrate = 0.0f;
            pid_roll_rate.integrate = 0.0f;
        }
        float po = PID_Update_Inner(&pid_pitch_rate, pitch_desired_rate, imu.gx,  CONTROL_DT);
        float ro = PID_Update_Inner(&pid_roll_rate,  roll_desired_rate,  -imu.gy, CONTROL_DT);
        if (integral_hold) {
            pid_pitch_rate.integrate = 0.0f;
            pid_roll_rate.integrate = 0.0f;
        }

        /* pitch_out不取反(与2807一致), roll_out不取反(内环-gy已自带取反) */
        if (phase1 || phase2) {
            mixer.pitch_out = po * fade;
            mixer.roll_out  = ro * fade;
            mixer.yaw_out   = 0;
        } else {
            mixer.pitch_out = po;
            mixer.roll_out  = ro;
            mixer.yaw_out   = 0;
        }

        /* PID自身尚未到输出上限时，四电机混控也可能先碰到1150/2000。
         * 饱和时撤销本周期中让积分绝对值继续增大的部分；反向卸载仍保留。 */
        mixer_saturated = Mixer_Would_Saturate(&mixer, mixer_lower_limit);
        if (mixer_saturated) {
            if (FastAbs(pid_pitch_rate.integrate) > FastAbs(pitch_i_before)) {
                po += pitch_i_before - pid_pitch_rate.integrate;
                pid_pitch_rate.integrate = pitch_i_before;
                pid_pitch_rate.control_output = constrain_float(
                    po, -pid_pitch_rate.control_output_limit,
                    pid_pitch_rate.control_output_limit);
            }
            if (FastAbs(pid_roll_rate.integrate) > FastAbs(roll_i_before)) {
                ro += roll_i_before - pid_roll_rate.integrate;
                pid_roll_rate.integrate = roll_i_before;
                pid_roll_rate.control_output = constrain_float(
                    ro, -pid_roll_rate.control_output_limit,
                    pid_roll_rate.control_output_limit);
            }
            mixer.pitch_out = (phase1 || phase2) ? po * fade : po;
            mixer.roll_out  = (phase1 || phase2) ? ro * fade : ro;
        }
        Mixer_Update(&mixer);
        if (debug_mode) {
            /* 调试模式: 电机全停, 只看传感器数据 */
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, 990);
            __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, 990);
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, 990);
            __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, 990);
        } else if (phase1) {
            Startup_Mixer_Output(&mixer);
        } else {
            Mixer_Output(&mixer);
        }
    }
    else
    {
        imu_fail++;
    }

    elapsed_cycles = DWT->CYCCNT - control_start_cycles;
    control_last_cycles = elapsed_cycles;
    if (elapsed_cycles > control_max_cycles) {
        control_max_cycles = elapsed_cycles;
    }
    if (elapsed_cycles > (SystemCoreClock / 200U)) {
        control_overrun++;
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
