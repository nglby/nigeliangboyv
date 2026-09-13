#include "bmi088.h"

extern I2C_HandleTypeDef hi2c1;   /* CubeMX 生成的 I2C 句柄 */
extern void MX_I2C1_Init(void);   /* 总线恢复时重新初始化 */

/* 当前量程配置（init 里设定，读取时用于换算）*/
static float acc_range_g  = 6.0f;    /* ±6g */
static float gyro_range_dps = 2000.0f; /* ±2000°/s */

/* ---------- I2C 总线恢复（防止卡死）---------- */
static void I2C_BusRecover(I2C_HandleTypeDef *hi2c)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 1. 释放 I2C，把 SCL/SDA 切成 GPIO 推挽输出 */
    HAL_I2C_DeInit(hi2c);

    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* 2. 发 9 个 SCL 时钟脉冲，释放可能被 SLAVE 拉死的 SDA */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_Delay(1);
    }

    /* 3. 发出 STOP 条件: SCL高→SDA从低变高 */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    /* 4. 恢复 I2C 功能 */
    MX_I2C1_Init();
}

/* ---------- 底层读写 ---------- */

static uint8_t BMI088_WriteReg(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                               uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(hi2c, dev_addr, reg,
                             I2C_MEMADD_SIZE_8BIT, &value, 1, 10);
}

static uint8_t BMI088_ReadRegs(I2C_HandleTypeDef *hi2c, uint16_t dev_addr,
                               uint8_t reg, uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(hi2c, dev_addr, reg,
                            I2C_MEMADD_SIZE_8BIT, buf, len, 2);
}

/* ---------- 初始化 ---------- */

uint8_t BMI088_Init(I2C_HandleTypeDef *hi2c)
{
    uint8_t id;
    uint8_t ret;
    int attempt;

    /* === 0. 等待 BMI088 上电稳定 + 总线恢复 === */
    HAL_Delay(100);
    I2C_BusRecover(hi2c);
    HAL_Delay(10);

    /* === 1. 读加速度计 WHO_AM_I（带重试）=== */
    for (attempt = 0; attempt < 5; attempt++) {
        ret = BMI088_ReadRegs(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_CHIP_ID, &id, 1);
        if (ret == HAL_OK && (id == 0x1A || id == 0x1E))
            break;
        HAL_Delay(10);
    }
    if (attempt >= 5)
        return 1;  /* 加速度计通信失败 */

    /* === 2. 读陀螺仪 WHO_AM_I（带重试）=== */
    for (attempt = 0; attempt < 5; attempt++) {
        ret = BMI088_ReadRegs(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_CHIP_ID, &id, 1);
        if (ret == HAL_OK && id == BMI088_GYRO_CHIP_ID_VAL)
            break;
        HAL_Delay(10);
    }
    if (attempt >= 5)
        return 2;  /* 陀螺仪通信失败 */

    /* === 3. 加速度计软复位 === */
    BMI088_WriteReg(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_SOFTRESET, 0xB6);
    HAL_Delay(50);

    /* === 4. 陀螺仪软复位 === */
    BMI088_WriteReg(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_SOFTRESET, 0xB6);
    HAL_Delay(50);

    /* === 5. 配置加速度计电源（先 PWR_CONF 再 PWR_CTRL）=== */
    BMI088_WriteReg(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_PWR_CONF, 0x00);  /* 退出休眠 */
    HAL_Delay(10);
    BMI088_WriteReg(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_PWR_CTRL, BMI088_ACC_PWR_ON);  /* 开传感器 */
    HAL_Delay(10);

    /* 量程 ±6g */
    BMI088_WriteReg(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_RANGE, BMI088_ACC_RANGE_6G);
    acc_range_g = 6.0f;
    /* Normal bandwidth, ODR 200Hz: acc_bwp=0xA, acc_odr=0x9 */
    BMI088_WriteReg(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_CONF, 0xA9);

    /* === 6. 配置陀螺仪 === */
    /* 量程 ±2000°/s */
    BMI088_WriteReg(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_RANGE, BMI088_GYRO_RANGE_2000);
    gyro_range_dps = 2000.0f;
    /* ODR 200Hz, 64Hz filter bandwidth (0x06).
     * 0x07 is only 100Hz ODR / 32Hz bandwidth. */
    BMI088_WriteReg(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_BANDWIDTH, 0x06);
    /* 正常电源模式 */
    BMI088_WriteReg(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_LPM1, 0x00);

    HAL_Delay(10);
    return 0;  /* 成功 */
}

/* ---------- 读原始数据 ---------- */

uint8_t BMI088_ReadAccelRaw(I2C_HandleTypeDef *hi2c, int16_t *ax, int16_t *ay, int16_t *az)
{
    uint8_t buf[6];
    uint8_t ret = BMI088_ReadRegs(hi2c, BMI088_ACCEL_ADDR, BMI088_ACC_DATA_8, buf, 6);
    if (ret != HAL_OK) return ret;

    /* BMI088 加速度计是完整的16位二补码数据，LSB先发。 */
    *ax = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *ay = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *az = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

    return HAL_OK;
}

uint8_t BMI088_ReadGyroRaw(I2C_HandleTypeDef *hi2c, int16_t *gx, int16_t *gy, int16_t *gz)
{
    uint8_t buf[6];
    uint8_t ret = BMI088_ReadRegs(hi2c, BMI088_GYRO_ADDR, BMI088_GYRO_DATA_8, buf, 6);
    if (ret != HAL_OK) return ret;

    /* BMI088 陀螺仪: 16位有符号，LSB先发 */
    *gx = (int16_t)(((uint16_t)buf[1] << 8) | buf[0]);
    *gy = (int16_t)(((uint16_t)buf[3] << 8) | buf[2]);
    *gz = (int16_t)(((uint16_t)buf[5] << 8) | buf[4]);

    return HAL_OK;
}

/* ---------- 读换算后数据（物理量）---------- */

uint8_t BMI088_ReadAccel(I2C_HandleTypeDef *hi2c, float *ax, float *ay, float *az)
{
    int16_t raw_x, raw_y, raw_z;
    uint8_t ret = BMI088_ReadAccelRaw(hi2c, &raw_x, &raw_y, &raw_z);
    if (ret != HAL_OK) return ret;

    /* ±range g，16位精度，半量程 = 32768 */
    /* 转成 m/s²: g值 × 9.80665 */
    float accel_scale = acc_range_g * 9.80665f / 32768.0f;
    *ax = (float)raw_x * accel_scale;
    *ay = (float)raw_y * accel_scale;
    *az = (float)raw_z * accel_scale;

    return HAL_OK;
}

uint8_t BMI088_ReadGyro(I2C_HandleTypeDef *hi2c, float *gx, float *gy, float *gz)
{
    int16_t raw_x, raw_y, raw_z;
    uint8_t ret = BMI088_ReadGyroRaw(hi2c, &raw_x, &raw_y, &raw_z);
    if (ret != HAL_OK) return ret;

    /* ±range °/s，16位精度，半量程 = 32768 */
    float lsb_per_dps = 32768.0f;
    *gx = (float)raw_x / lsb_per_dps * gyro_range_dps;
    *gy = (float)raw_y / lsb_per_dps * gyro_range_dps;
    *gz = (float)raw_z / lsb_per_dps * gyro_range_dps;

    return HAL_OK;
}

uint8_t BMI088_ReadAll(I2C_HandleTypeDef *hi2c, BMI088_Data_t *data)
{
    uint8_t ret = BMI088_ReadAccel(hi2c, &data->ax, &data->ay, &data->az);
    if (ret != HAL_OK) return ret;
    ret = BMI088_ReadGyro(hi2c, &data->gx, &data->gy, &data->gz);
    return ret;
}
