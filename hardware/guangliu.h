#ifndef GUANGLIU_H
#define GUANGLIU_H

#include "main.h"
#include "usart.h"

/* 环形缓冲区 */
#define RINGBUFF_LEN 50

typedef struct {
    uint16_t Head;
    uint16_t Tail;
    uint8_t  Ring_Buff[RINGBUFF_LEN];
} RingBuff_t;

/* LC306/307 原始数据帧 */
typedef struct {
    int16_t  pixel_flow_x_integral;  /* X轴像素累计位移 (radians*10000) */
    int16_t  pixel_flow_y_integral;  /* Y轴像素累计位移 (radians*10000) */
    uint16_t integration_timespan;   /* 累计时间 (us) */
    uint8_t  qual;                   /* 地面质量 0-255 */
} flow_integral_frame;

/* 光流计算结果 */
typedef struct {
    float x;          /* X轴实际位移 (mm) 原始 */
    float y;          /* Y轴实际位移 (mm) 原始 */
    float x_filt;     /* X轴滤波后位移 (mm) */
    float y_filt;     /* Y轴滤波后位移 (mm) */
    uint16_t dt;      /* 时间间隔 (ms) */
    uint8_t  qual;    /* 地面质量 */
    uint8_t  update;  /* 数据更新标志 */
} flow_float;

/* 外部变量 */
extern flow_integral_frame opt_origin_data;
extern flow_float          opt_data;
extern uint8_t             OpticalFlow_Is_Work;
extern RingBuff_t          OpticalFlow_Ringbuf;
extern volatile uint32_t   raw_byte_cnt;  /* 调试用：收到的原始字节总数 */
extern volatile uint32_t   uart_err_cnt;  /* 调试用：UART错误次数 */
extern volatile uint32_t   uart_last_err; /* 调试用：最近一次UART错误码 */

/* 函数声明 */
void    OpticalFlow_Init(void);
uint8_t Optflow_Prase(void);
uint8_t LC307_Config_Init(void);
void    RingBuff_Init(RingBuff_t *ringBuff);
void    RingBuf_Write(uint8_t data, RingBuff_t *ringBuff, uint16_t Length);

#endif
