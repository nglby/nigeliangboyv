#ifndef __PID_H
#define __PID_H

// ==================== PID结构体 ====================
// 一个结构体管一个电机，里面存着PID需要的所有东西
typedef struct
{
    // --- 三个参数（需要你调的）---
    float Kp;           // 比例系数：误差越大，输出越大
    float Ki;           // 积分系数：消除稳态误差（速度一直差一点的时候）
    float Kd;           // 微分系数：抑制超调（速度冲过头的时候往回拉）

    // --- 运行时变量（代码自己算，不用管）---
    float target;       // 目标值（你想让电机转多快 / 转到哪）
    float feedback;      // 实际值（电机当前转速 / 当前角度）
    float error;         // 这次误差 = target - feedback
    float last_error;    // 上次误差（算D用）
    float integral;      // 误差累积（算I用）
    float output;        // PID算出来的输出

    // --- 限制（防飞车）---
    float integral_limit;   // 积分上限，防止积分项越积越大
    float output_limit;     // 输出上限

} PID_TypeDef;

// ==================== 函数声明 ====================
void  PID_Init(PID_TypeDef *pid, float kp, float ki, float kd, float integral_limit, float output_limit);
float PID_Calculate(PID_TypeDef *pid, float target, float feedback);

#endif
