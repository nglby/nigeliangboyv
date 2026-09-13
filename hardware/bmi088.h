#ifndef BMI088_H
#define BMI088_H

#include "main.h"
#include <stdint.h>

/* I2C 地址（HAL 用 8 位格式 = 7位地址 << 1）*/
/* 模块 SDO 上拉，加速度计用 SECONDARY 地址 0x19 */
#define BMI088_ACCEL_ADDR    ((uint16_t)(0x19 << 1))   /* 0x32 */
#define BMI088_GYRO_ADDR     ((uint16_t)(0x69 << 1))   /* 0xD2 */

/* ========== 加速度计寄存器 ========== */
#define BMI088_ACC_CHIP_ID       0x00
#define BMI088_ACC_CHIP_ID_VAL   0x1E   /* BMI088 SECONDARY, 兼容 0x1A */

#define BMI088_ACC_DATA_8        0x12   /* ACC_X_LSB，连续读6字节 */
#define BMI088_ACC_CONF          0x40   /* ODR & BW */
#define BMI088_ACC_RANGE         0x41   /* 量程 */
#define BMI088_ACC_PWR_CONF      0x7C   /* 电源配置 */
#define BMI088_ACC_PWR_CTRL      0x7D   /* 电源控制 */
#define BMI088_ACC_SOFTRESET     0x7E   /* 写 0xB6 复位 */

/* ACC_PWR_CTRL 值 */
#define BMI088_ACC_PWR_OFF       0x00
#define BMI088_ACC_PWR_ON        0x04   /* active mode */

/* ACC_RANGE 值 */
#define BMI088_ACC_RANGE_3G      0x00
#define BMI088_ACC_RANGE_6G      0x01
#define BMI088_ACC_RANGE_12G     0x02
#define BMI088_ACC_RANGE_24G     0x03

/* ========== 陀螺仪寄存器 ========== */
#define BMI088_GYRO_CHIP_ID      0x00
#define BMI088_GYRO_CHIP_ID_VAL  0x0F   /* WHO_AM_I 期望值 */

#define BMI088_GYRO_DATA_8       0x02   /* RATE_X_LSB，连续读6字节 */
#define BMI088_GYRO_RANGE        0x0F   /* 量程 */
#define BMI088_GYRO_BANDWIDTH    0x10   /* ODR & filter */
#define BMI088_GYRO_LPM1         0x11   /* 电源模式 */
#define BMI088_GYRO_SOFTRESET    0x14   /* 写 0xB6 复位 */
#define BMI088_GYRO_INT_CTRL     0x15

/* GYRO_RANGE 值 */
#define BMI088_GYRO_RANGE_2000   0x00   /* ±2000 °/s */
#define BMI088_GYRO_RANGE_1000   0x01
#define BMI088_GYRO_RANGE_500    0x02
#define BMI088_GYRO_RANGE_250    0x03
#define BMI088_GYRO_RANGE_125    0x04

/* ========== 数据结构 ========== */
typedef struct {
    float ax, ay, az;    /* 加速度 m/s² */
    float gx, gy, gz;    /* 角速度 °/s   */
    float temp;          /* 温度 °C       */
} BMI088_Data_t;

/* ========== 函数声明 ========== */
uint8_t BMI088_Init(I2C_HandleTypeDef *hi2c);
uint8_t BMI088_ReadAccel(I2C_HandleTypeDef *hi2c, float *ax, float *ay, float *az);
uint8_t BMI088_ReadGyro(I2C_HandleTypeDef *hi2c, float *gx, float *gy, float *gz);
uint8_t BMI088_ReadAll(I2C_HandleTypeDef *hi2c, BMI088_Data_t *data);
uint8_t BMI088_ReadAccelRaw(I2C_HandleTypeDef *hi2c, int16_t *ax, int16_t *ay, int16_t *az);
uint8_t BMI088_ReadGyroRaw(I2C_HandleTypeDef *hi2c, int16_t *gx, int16_t *gy, int16_t *gz);

#endif /* BMI088_H */
