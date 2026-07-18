#ifndef __HWT605_H
#define __HWT605_H

#include "main.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
typedef struct 
{
	float ax,ay,az;
	float gx,gy,gz;
	float roll, pitch, yaw;
  float temp; 
  uint8_t acc_ready;    
  uint8_t gyro_ready;    
  uint8_t angle_ready;  
}hwt605_data_t;

hwt605_data_t hwt605;

void hwt605_init(void);

#endif