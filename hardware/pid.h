#ifndef PID_H
#define PID_H

#include "main.h"
#include <string.h>

#define MOTOR_MIN  1150.0f
#define MOTOR_MAX  2000.0f

/* ===== 巴特沃斯2阶低通滤波器 (移植自官方枫叶飞控) ===== */
typedef struct {
    float input[3];
    float output[3];
} filter_buffer;

typedef struct {
    float b[3];
    float a[3];
} filter_parameter;

/* ===== PID控制器结构 (移植自官方pid_ctrl, 精简为姿态控制所需) ===== */
typedef struct {
    /* 限幅标志 */
    uint8_t err_limit_flag;            /* 偏差限幅标志 */
    uint8_t integrate_limit_flag;      /* 积分限幅标志 */
    uint8_t integrate_separation_flag; /* 积分分离标志 */

    /* 输入输出 */
    float expect;                      /* 期望 */
    float feedback;                    /* 反馈值 */

    /* 偏差 */
    float err;                         /* 当前偏差 */
    float last_err;                    /* 上次偏差 */
    float pre_last_err;               /* 上上次偏差 */
    float err_max;                     /* 偏差限幅值 */

    /* 积分 */
    float integrate;                   /* 积分值 */
    float integrate_max;               /* 积分限幅值 */
    float integrate_separation_err;    /* 积分分离偏差值 */

    /* PID参数 */
    float kp, ki, kd;

    /* 输出 */
    float control_output;              /* 控制器总输出 */
    float last_control_output;         /* 上次输出 */
    float control_output_limit;        /* 输出限幅 */

    /* 微分项 (巴特沃斯滤波) */
    float dis_err;                     /* 误差微分量 */
    float dis_err_lpf;                 /* 滤波后微分量 */
    float dis_error_history[5];        /* 历史微分量 */
    float last_derivative;             /* 上次微分量(一阶LPF) */

    /* 滤波器 */
    filter_buffer lpf_buf;             /* 低通滤波缓冲 */
    filter_parameter lpf_param;        /* 低通滤波参数 */
    float d_lpf_alpha;                 /* D项一阶低通系数 */
} PID_t;

/* ===== 滤波器函数 ===== */
void set_cutoff_frequency(float sample_freq, float cutoff_freq, filter_parameter *lpf);
float butterworth(float curr_input, filter_buffer *buf, filter_parameter *lpf);
void set_d_lpf_alpha(int16_t cutoff_freq, float time_step, float *alpha);

/* ===== 数学工具 ===== */
float constrain_float(float amt, float low, float high);
float FastAbs(float x);

/* ===== PID函数 ===== */
/* 外环(角度环): 简单PID, 等效官方pid_ctrl_general */
void PID_Init_Outer(PID_t *pid, float kp, float ki, float kd,
                    float err_max, float integ_max, float out_limit);
float PID_Update_Outer(PID_t *pid, float expect, float feedback, float dt);

/* 内环(角速度环): PID+巴特沃斯D项滤波, 等效官方pid_ctrl_div_lpf */
void PID_Init_Inner(PID_t *pid, float kp, float ki, float kd,
                    float err_max, float integ_max, float out_limit,
                    float sample_freq, float cutoff_freq);
float PID_Update_Inner(PID_t *pid, float expect, float feedback, float dt);

void PID_Reset(PID_t *pid);

/* ===== Mixer (不变) ===== */
typedef struct {
    float throttle;
    float pitch_out;
    float roll_out;
    float yaw_out;
    float m1, m2, m3, m4;
} Mixer_t;

void Mixer_Update(Mixer_t *m);
void Mixer_Update_Limited(Mixer_t *m, float lower_limit);
void Mixer_Output(Mixer_t *m);

#endif
