#include "mpu6050.h"
#include "i2c.h"
void mpu6050_init()
{
	HAL_Delay(100);
	uint8_t sendaddress = 0x6b;
	uint8_t senddata = 0x00;
	HAL_I2C_Mem_Write(&hi2c1,0xd1,sendaddress,1,&senddata,1,0xff);
	
	sendaddress = 0x19;
	senddata = 0x07;
	HAL_I2C_Mem_Write(&hi2c1,0xd1,sendaddress,1,&senddata,1,0xff);
	
	sendaddress = 0x1A;
	senddata = 0x06;
	HAL_I2C_Mem_Write(&hi2c1,0xd1,sendaddress,1,&senddata,1,0xff);
	
	sendaddress = 0x1B;
	senddata = 0x08;
	HAL_I2C_Mem_Write(&hi2c1,0xd1,sendaddress,1,&senddata,1,0xff);
	
	sendaddress = 0x1C;
	senddata = 0x00;
	HAL_I2C_Mem_Write(&hi2c1,0xd1,sendaddress,1,&senddata,1,0xff);
	
}