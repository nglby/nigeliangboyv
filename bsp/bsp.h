#ifndef __BSP_H
#define __BSP_H
#include "main.h"
#include "pid.h"

extern CAN_TxHeaderTypeDef CAN1_Tx_Hander;
extern uint8_t TxData[8];
extern uint32_t TxMailbox;
extern volatile int16_t moto_angle;
extern volatile int16_t moto_speed;
extern volatile int16_t moto_current;

extern PID_TypeDef moto1_pid;       // 速度PID（内层）
extern PID_TypeDef moto1_pos_pid;   // 位置PID（外层）
extern volatile int16_t speed;       // 目标转速(RPM)，在main.c里定义，0=位置保持，非0=速度模式
extern int32_t moto1_total_angle;    // 连续角度（不回绕），只读
extern int32_t moto1_pos_hold;       // 位置保持目标

#endif
