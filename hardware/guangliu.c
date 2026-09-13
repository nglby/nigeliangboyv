#include "guangliu.h"
#include <stdio.h>

/* ================ 全局变量 ================ */
uint8_t             OpticalFlow_Is_Work = 0;
RingBuff_t          OpticalFlow_Ringbuf;
flow_integral_frame opt_origin_data;
flow_float          opt_data;
float               opticalflow_high = 1000.0f;  /* 默认高度 1m = 1000mm */
volatile uint32_t   raw_byte_cnt = 0;  /* 调试用：收到的原始字节总数 */
volatile uint32_t   uart_err_cnt = 0;  /* 调试用：UART错误次数 */
volatile uint32_t   uart_last_err = 0; /* 调试用：最近一次UART错误码 */

static uint8_t rx_byte;  /* 中断接收缓冲 */

/* ================ LC306/307 配置参数 ================ */
#define SENSOR_IIC_ADDR 0xdc

static const uint8_t tab_focus[4] = {0x96, 0x26, 0xbc, 0x50};

static const uint8_t Sensor_cfg[] = {
  0x12, 0x80,
  0x11, 0x30,
  0x1b, 0x06,
  0x6b, 0x43,
  0x12, 0x20,
  0x3a, 0x00,
  0x15, 0x02,
  0x62, 0x81,
  0x08, 0xa0,
  0x06, 0x68,
  0x2b, 0x20,
  0x92, 0x25,
  0x27, 0x97,
  0x17, 0x01,
  0x18, 0x79,
  0x19, 0x00,
  0x1a, 0xa0,
  0x03, 0x00,
  0x13, 0x00,
  0x01, 0x13,
  0x02, 0x20,
  0x87, 0x16,
  0x8c, 0x01,
  0x8d, 0xcc,
  0x13, 0x07,
  0x33, 0x10,
  0x34, 0x1d,
  0x35, 0x46,
  0x36, 0x40,
  0x37, 0xa4,
  0x38, 0x7c,
  0x65, 0x46,
  0x66, 0x46,
  0x6e, 0x20,
  0x9b, 0xa4,
  0x9c, 0x7c,
  0xbc, 0x0c,
  0xbd, 0xa4,
  0xbe, 0x7c,
  0x20, 0x09,
  0x09, 0x03,
  0x72, 0x2f,
  0x73, 0x2f,
  0x74, 0xa7,
  0x75, 0x12,
  0x79, 0x8d,
  0x7a, 0x00,
  0x7e, 0xfa,
  0x70, 0x0f,
  0x7c, 0x84,
  0x7d, 0xba,
  0x5b, 0xc2,
  0x76, 0x90,
  0x7b, 0x55,
  0x71, 0x46,
  0x77, 0xdd,
  0x13, 0x0f,
  0x8a, 0x10,
  0x8b, 0x20,
  0x8e, 0x21,
  0x8f, 0x40,
  0x94, 0x41,
  0x95, 0x7e,
  0x96, 0x7f,
  0x97, 0xf3,
  0x13, 0x07,
  0x24, 0x58,
  0x97, 0x48,
  0x25, 0x08,
  0x94, 0xb5,
  0x95, 0xc0,
  0x80, 0xf4,
  0x81, 0xe0,
  0x82, 0x1b,
  0x83, 0x37,
  0x84, 0x39,
  0x85, 0x58,
  0x86, 0xff,
  0x89, 0x15,
  0x8a, 0xb8,
  0x8b, 0x99,
  0x39, 0x98,
  0x3f, 0x98,
  0x90, 0xa0,
  0x91, 0xe0,
  0x40, 0x20,
  0x41, 0x28,
  0x42, 0x26,
  0x43, 0x25,
  0x44, 0x1f,
  0x45, 0x1a,
  0x46, 0x16,
  0x47, 0x12,
  0x48, 0x0f,
  0x49, 0x0d,
  0x4b, 0x0b,
  0x4c, 0x0a,
  0x4e, 0x08,
  0x4f, 0x06,
  0x50, 0x06,
  0x5a, 0x56,
  0x51, 0x1b,
  0x52, 0x04,
  0x53, 0x4a,
  0x54, 0x26,
  0x57, 0x75,
  0x58, 0x2b,
  0x5a, 0xd6,
  0x51, 0x28,
  0x52, 0x1e,
  0x53, 0x9e,
  0x54, 0x70,
  0x57, 0x50,
  0x58, 0x07,
  0x5c, 0x28,
  0xb0, 0xe0,
  0xb1, 0xc0,
  0xb2, 0xb0,
  0xb3, 0x4f,
  0xb4, 0x63,
  0xb4, 0xe3,
  0xb1, 0xf0,
  0xb2, 0xa0,
  0x55, 0x00,
  0x56, 0x40,
  0x96, 0x50,
  0x9a, 0x30,
  0x6a, 0x81,
  0x23, 0x33,
  0xa0, 0xd0,
  0xa1, 0x31,
  0xa6, 0x04,
  0xa2, 0x0f,
  0xa3, 0x2b,
  0xa4, 0x0f,
  0xa5, 0x2b,
  0xa7, 0x9a,
  0xa8, 0x1c,
  0xa9, 0x11,
  0xaa, 0x16,
  0xab, 0x16,
  0xac, 0x3c,
  0xad, 0xf0,
  0xae, 0x57,
  0xc6, 0xaa,
  0xd2, 0x78,
  0xd0, 0xb4,
  0xd1, 0x00,
  0xc8, 0x10,
  0xc9, 0x12,
  0xd3, 0x09,
  0xd4, 0x2a,
  0xee, 0x4c,
  0x7e, 0xfa,
  0x74, 0xa7,
  0x78, 0x4e,
  0x60, 0xe7,
  0x61, 0xc8,
  0x6d, 0x70,
  0x1e, 0x39,
  0x98, 0x1a
};

/* ================ 底层 UART 操作（配置阶段用，直接寄存器，跟LC306例程一致） ================ */

static void Uart2_SendByte(uint8_t dat)
{
    while (!__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE));
    huart2.Instance->DR = dat;
}

static uint8_t Uart2_WaitByte(uint32_t timeout_ms)
{
    uint32_t tickstart = HAL_GetTick();
    while (!__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        if ((HAL_GetTick() - tickstart) > timeout_ms)
            return 0xFF;  /* 超时 */
    }
    return (uint8_t)(huart2.Instance->DR & 0xFF);
}

/* ================ 环形缓冲区 ================ */

void RingBuff_Init(RingBuff_t *ringBuff)
{
    ringBuff->Head = 0;
    ringBuff->Tail = 0;
}

void RingBuf_Write(uint8_t data, RingBuff_t *ringBuff, uint16_t Length)
{
    ringBuff->Ring_Buff[ringBuff->Tail] = data;
    if (++ringBuff->Tail >= Length)
        ringBuff->Tail = 0;
    if (ringBuff->Tail == ringBuff->Head) {
        if (++ringBuff->Head >= Length)
            ringBuff->Head = 0;
    }
}

/* ================ LC307 模块配置初始化 ================ */

uint8_t LC307_Config_Init(void)
{
    uint16_t i;
    uint16_t len = sizeof(Sensor_cfg);
    uint8_t  recv[3];

    HAL_Delay(100);  /* 上电后至少延时 100ms */

    /* 1) 0xAA 开启配置（无应答） */
    Uart2_SendByte(0xAA);

    /* 2) 0xAB 模块参数配置 */
    Uart2_SendByte(0xAB);
    Uart2_SendByte(tab_focus[0]);
    Uart2_SendByte(tab_focus[1]);
    Uart2_SendByte(tab_focus[2]);
    Uart2_SendByte(tab_focus[3]);
    Uart2_SendByte(tab_focus[0] ^ tab_focus[1] ^ tab_focus[2] ^ tab_focus[3]);

    /* 等待模块返回 3 字节: 0xAB, 状态值, XOR */
    recv[0] = Uart2_WaitByte(100);
    recv[1] = Uart2_WaitByte(100);
    recv[2] = Uart2_WaitByte(100);
    if (((recv[0] ^ recv[1]) == recv[2]) && (recv[1] == 0x00)) {
        printf("LC307 AB config OK\r\n");
    } else {
        printf("LC307 AB config FAIL: %02X %02X %02X\r\n", recv[0], recv[1], recv[2]);
        return 0;
    }

    /* 3) 0xBB 传感器寄存器配置（循环发送每一组地址+数据） */
    for (i = 0; i < len; i += 2) {
        Uart2_SendByte(0xBB);
        Uart2_SendByte(SENSOR_IIC_ADDR);
        Uart2_SendByte(Sensor_cfg[i]);
        Uart2_SendByte(Sensor_cfg[i + 1]);
        Uart2_SendByte(SENSOR_IIC_ADDR ^ Sensor_cfg[i] ^ Sensor_cfg[i + 1]);

        recv[0] = Uart2_WaitByte(50);
        recv[1] = Uart2_WaitByte(50);
        recv[2] = Uart2_WaitByte(50);
        if (((recv[0] ^ recv[1]) == recv[2]) && (recv[1] == 0x00)) {
            /* 该寄存器配置成功 */
        } else {
            printf("LC307 BB config FAIL at 0x%02X\r\n", Sensor_cfg[i]);
            return 0;
        }
    }

    /* 4) 0xDD 关闭配置（无应答） */
    Uart2_SendByte(0xDD);
    printf("LC307 config success\r\n");
    return 1;
}

/* ================ 光流初始化 ================ */

void OpticalFlow_Init(void)
{
    RingBuff_Init(&OpticalFlow_Ringbuf);

    /* 关键：配置期间必须关掉 USART2 NVIC 中断！
     * 否则 HAL_UART_IRQHandler 会把模块返回的数据读走丢弃，
     * 导致 Uart2_WaitByte 轮询 RXNE 永远超时。
     * LC306 例程也是在 NVIC 使能之前做配置的。 */
    HAL_NVIC_DisableIRQ(USART2_IRQn);

    OpticalFlow_Is_Work = LC307_Config_Init();

    /* 配置完成，重新使能 NVIC 中断，开启中断接收 */
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_Delay(500);  /* 等待 128 帧图像后开始输出数据 */
    HAL_StatusTypeDef rx_ret = HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    printf("RX_IT init: %d (0=OK)\r\n", rx_ret);
    printf("Optical Flow ready, Is_Work=%d\r\n", OpticalFlow_Is_Work);
}

/* ================ 滤波参数 ================ */
#define FLOW_FILTER_SIZE  8   /* 滑动平均窗口：最近8帧取平均 */
#define FLOW_DEADZONE     3   /* 死区阈值：|值|<3 视为噪声归零 */

static int16_t flow_x_hist[FLOW_FILTER_SIZE];
static int16_t flow_y_hist[FLOW_FILTER_SIZE];
static uint8_t flow_hist_idx = 0;
static uint8_t flow_hist_cnt = 0;

/* ================ LC306/307 数据帧解析 ================ */
/*
 * 帧格式 (14 字节):
 * [0]  0xFE   帧头
 * [1]  0x0A   帧长
 * [2]  flow_x_integral 低字节
 * [3]  flow_x_integral 高字节
 * [4]  flow_y_integral 低字节
 * [5]  flow_y_integral 高字节
 * [6]  integration_timespan 低字节
 * [7]  integration_timespan 高字节
 * [8-11] 未使用
 * [12] qual   地面质量
 * [13] 0x55   帧尾
 */
uint8_t Optflow_Prase(void)
{
    uint16_t i;
    int16_t raw_x, raw_y;
    int32_t sum_x, sum_y;
    uint8_t j;

    for (i = 0; i <= RINGBUFF_LEN - 14; i++) {
        if (OpticalFlow_Ringbuf.Ring_Buff[i]     == 0xFE &&
            OpticalFlow_Ringbuf.Ring_Buff[i + 1]  == 0x0A &&
            OpticalFlow_Ringbuf.Ring_Buff[i + 13] == 0x55) {

            /* 提取原始数据（小端序，低字节在前） */
            raw_x = (int16_t)((OpticalFlow_Ringbuf.Ring_Buff[i + 3] << 8) | OpticalFlow_Ringbuf.Ring_Buff[i + 2]);
            raw_y = (int16_t)((OpticalFlow_Ringbuf.Ring_Buff[i + 5] << 8) | OpticalFlow_Ringbuf.Ring_Buff[i + 4]);

            opt_origin_data.pixel_flow_x_integral = raw_x;
            opt_origin_data.pixel_flow_y_integral = raw_y;
            opt_origin_data.integration_timespan =
                (uint16_t)((OpticalFlow_Ringbuf.Ring_Buff[i + 7] << 8) | OpticalFlow_Ringbuf.Ring_Buff[i + 6]);
            opt_origin_data.qual = OpticalFlow_Ringbuf.Ring_Buff[i + 12];

            /* 死区滤波：静止时噪声归零 */
            if (raw_x > -FLOW_DEADZONE && raw_x < FLOW_DEADZONE) raw_x = 0;
            if (raw_y > -FLOW_DEADZONE && raw_y < FLOW_DEADZONE) raw_y = 0;

            /* 存入滑动窗口 */
            flow_x_hist[flow_hist_idx] = raw_x;
            flow_y_hist[flow_hist_idx] = raw_y;
            if (++flow_hist_idx >= FLOW_FILTER_SIZE) flow_hist_idx = 0;
            if (flow_hist_cnt < FLOW_FILTER_SIZE) flow_hist_cnt++;

            /* 滑动平均 */
            sum_x = 0; sum_y = 0;
            for (j = 0; j < flow_hist_cnt; j++) {
                sum_x += flow_x_hist[j];
                sum_y += flow_y_hist[j];
            }

            /* 计算实际位移 (mm) = 像素位移 * 高度(mm) / 10000 */
            opt_data.x = (opt_origin_data.pixel_flow_x_integral * opticalflow_high) / 10000.0f;
            opt_data.y = (opt_origin_data.pixel_flow_y_integral * opticalflow_high) / 10000.0f;
            opt_data.x_filt = ((float)sum_x / flow_hist_cnt * opticalflow_high) / 10000.0f;
            opt_data.y_filt = ((float)sum_y / flow_hist_cnt * opticalflow_high) / 10000.0f;
            opt_data.dt    = (uint16_t)(opt_origin_data.integration_timespan * 0.001f);
            opt_data.qual  = opt_origin_data.qual;
            opt_data.update = 1;

            return 1;
        }
    }
    return 0;
}

/* ================ USART2 接收中断回调 ================ */

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        raw_byte_cnt++;
        RingBuf_Write(rx_byte, &OpticalFlow_Ringbuf, RINGBUFF_LEN);
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}

/* UART出错时HAL会调这个而不是RxCpltCallback，不处理的话接收就停死 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2) {
        uart_err_cnt++;
        uart_last_err = huart->ErrorCode;
        /* 清掉overrun等错误标志 */
        __HAL_UART_CLEAR_OREFLAG(huart);
        /* 重新启动接收 */
        HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    }
}
