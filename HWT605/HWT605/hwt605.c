#include "hwt605.h"

static uint8_t rx_idx   = 0;
static uint8_t rx_state = 0;   
static uint8_t rx_byte;   
extern uint8_t r_buf[11];

void hwt605_init(void)
{
	rx_idx = 0;
	rx_state = 0;
	HAL_UART_Receive_IT(&huart2,&rx_byte,1);
}

static int16_t make_short(uint8_t low, uint8_t high)
{
    return (int16_t)(((int16_t)high << 8) | low);
}

static uint8_t check_sum(uint8_t *buf)
{
    uint8_t i, sum = 0;
    for (i = 0; i < 11 - 1; i++)
        sum += buf[i];
    return sum;
	
}

static void HWT605_ParseFrame(uint8_t *buf)
{
    uint8_t type = buf[1];
    uint8_t sum  = check_sum(buf);

    if (sum != buf[11 - 1])
        return; 

    int16_t d1, d2, d3, d4;
    d1 = make_short(buf[2], buf[3]);
    d2 = make_short(buf[4], buf[5]);
    d3 = make_short(buf[6], buf[7]);
    d4 = make_short(buf[8], buf[9]);

    switch (type) {
    case 0x51: 
        hwt605.ax   = (float)d1 / 32768.0f * 16.0f; 
        hwt605.ay   = (float)d2 / 32768.0f * 16.0f;
        hwt605.az   = (float)d3 / 32768.0f * 16.0f;
        hwt605.temp = (float)d4 / 100.0f;            
        hwt605.acc_ready = 1;
        break;

    case 0x52: 
        hwt605.gx = (float)d1 / 32768.0f * 2000.0f; 
        hwt605.gy = (float)d2 / 32768.0f * 2000.0f;
        hwt605.gz = (float)d3 / 32768.0f * 2000.0f;
        hwt605.gyro_ready = 1;
        break;

    case 0x53:
        hwt605.roll  = (float)d1 / 32768.0f * 180.0f;
        hwt605.pitch = (float)d2 / 32768.0f * 180.0f;
        hwt605.yaw   = (float)d3 / 32768.0f * 180.0f;
        hwt605.angle_ready = 1;
        break;

    default:
        break;
    }
}


void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	if(huart->Instance == USART2)
	{
		uint8_t byte = rx_byte;
		if(rx_state == 0)
		{
			if(byte == 0x55)
			{
				r_buf[0] = 0x55;
				rx_idx = 1;
				rx_state = 1;
			}
			
		}
		else
		{
			r_buf[rx_idx++] = byte;
			if(rx_idx >= 11)
			{
				HWT605_ParseFrame(r_buf);
				rx_idx = 0;
				rx_state = 0;
			}
		}
	}
	
	HAL_UART_Receive_IT(&huart2,&rx_byte,1);
}

void send_pc(void)
{
////	if(hwt605.acc_ready == 1)
////	{
////		printf("%.3f,%.3f,%.3f\n",hwt605.ax,hwt605.ay,hwt605.az);
////		hwt605.acc_ready  =0;
////	}
////	if(hwt605.gyro_ready == 1)
////	{
////		printf("%.3f,%.3f,%.3f\n",hwt605.gx,hwt605.gy,hwt605.gz);
////		hwt605.gyro_ready =0;
////	}
	if(hwt605.angle_ready == 1)
	{
		printf("%.3f,%.3f,%.3f\n",hwt605.roll,hwt605.pitch,hwt605.yaw);
		hwt605.angle_ready = 0;
	}
	
}

