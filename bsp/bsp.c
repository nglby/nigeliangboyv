/*
name : 你哥梁博宇
*/

#include "bsp.h"

PID_TypeDef moto1_pid;      
PID_TypeDef moto1_pos_pid;  

int32_t moto1_total_angle = 0;
int32_t moto1_pos_hold = 0;  
static int16_t last_angle = -1;
static uint8_t first_angle_received = 0;
static int16_t last_speed = 0;

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	if(htim->Instance == TIM6)
	{
		float target_speed;
		
		if(last_speed != 0 && speed == 0)//跑着跑着变成0
		{
			moto1_pos_hold = moto1_total_angle;
			moto1_pid.integral = 0;   // 清速度PID积分，防冲击
		}
		if(last_speed == 0 && speed != 0)//从0起步
		{
			moto1_pos_pid.integral = 0;
		}
		last_speed = speed;

		if(speed != 0)
		{
		
			moto1_pos_hold += (int32_t)((float)speed * 8191.0f / 60000.0f);
			target_speed = (float)speed + PID_Calculate(&moto1_pos_pid, (float)moto1_pos_hold, (float)moto1_total_angle);
		}
		else
		{
			target_speed = PID_Calculate(&moto1_pos_pid, (float)moto1_pos_hold, (float)moto1_total_angle);
		}

		float output = PID_Calculate(&moto1_pid, target_speed, (float)moto_speed);

		int16_t current = (int16_t)output;
		TxData[0] = (current >> 8) & 0xFF;
		TxData[1] =  current       & 0xFF;

		HAL_CAN_AddTxMessage(&hcan1, &CAN1_Tx_Hander, TxData, &TxMailbox);
	}
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxHeaderTypeDef rx_header;
	uint8_t rx_data[8];
	if(HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &rx_header, rx_data) == HAL_OK)
	{
		if(rx_header.StdId == 0x201) 
		{
			int16_t new_angle = (int16_t)((rx_data[0] << 8) | rx_data[1]);

			if(first_angle_received == 0)
			{
				last_angle = new_angle;
				moto1_total_angle = new_angle;
				moto1_pos_hold = moto1_total_angle;
				first_angle_received = 1;
			}
			else
			{
				int16_t delta = new_angle - last_angle;
				if(delta >  4095) delta -= 8192;
				if(delta < -4095) delta += 8192;
				moto1_total_angle += delta;
				last_angle = new_angle;
			}
			moto_angle   = new_angle;
			moto_speed   = (int16_t)((rx_data[2] << 8) | rx_data[3]);
			moto_current = (int16_t)((rx_data[4] << 8) | rx_data[5]);
		}
	}
}