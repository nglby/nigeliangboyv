#include "pid.h"
#include "tim.h"
#include <math.h>

#ifndef M_PI_F
#define M_PI_F 3.141592653589793f
#endif

/* ===== 数学工具 (移植自官方wp_math) ===== */

float constrain_float(float amt, float low, float high)
{
    if (amt > high) return high;
    if (amt < low) return low;
    return amt;
}

float FastAbs(float x)
{
    return x >= 0.0f ? x : -x;
}

/* ===== 巴特沃斯2阶低通滤波器 (移植自官方filter.c) ===== */

void set_cutoff_frequency(float sample_freq, float cutoff_freq, filter_parameter *lpf)
{
    float fr = sample_freq / cutoff_freq;
    float ohm = tanf(M_PI_F / fr);
    float c = 1.0f + 2.0f * cosf(M_PI_F / 4.0f) * ohm + ohm * ohm;
    if (cutoff_freq <= 0.0f) {
        return;
    }
    lpf->b[0] = ohm * ohm / c;
    lpf->b[1] = 2.0f * lpf->b[0];
    lpf->b[2] = lpf->b[0];
    lpf->a[0] = 1.0f;
    lpf->a[1] = 2.0f * (ohm * ohm - 1.0f) / c;
    lpf->a[2] = (1.0f - 2.0f * cosf(M_PI_F / 4.0f) * ohm + ohm * ohm) / c;
}

float butterworth(float curr_input, filter_buffer *buf, filter_parameter *lpf)
{
    buf->input[2] = curr_input;
    buf->output[2] = lpf->b[0] * buf->input[2]
                   + lpf->b[1] * buf->input[1]
                   + lpf->b[2] * buf->input[0]
                   - lpf->a[1] * buf->output[1]
                   - lpf->a[2] * buf->output[0];
    buf->input[0] = buf->input[1];
    buf->input[1] = buf->input[2];
    buf->output[0] = buf->output[1];
    buf->output[1] = buf->output[2];

    uint16_t i;
    for (i = 0; i < 3; i++) {
        if (isnan(buf->output[i]) == 1)
            buf->output[i] = curr_input;
        if (isnan(buf->input[i]) == 1)
            buf->input[i] = curr_input;
    }
    return buf->output[2];
}

void set_d_lpf_alpha(int16_t cutoff_freq, float time_step, float *alpha)
{
    float rc = 1.0f / (2.0f * M_PI_F * cutoff_freq);
    *alpha = time_step / (time_step + rc);
}

/* ===== PID初始化 ===== */

static void PID_Init_Base(PID_t *pid)
{
    pid->err_limit_flag = 1;
    pid->integrate_limit_flag = 1;
    pid->integrate_separation_flag = 0;

    pid->expect = 0.0f;
    pid->feedback = 0.0f;

    pid->err = 0.0f;
    pid->last_err = 0.0f;
    pid->pre_last_err = 0.0f;
    pid->err_max = 0.0f;

    pid->integrate = 0.0f;
    pid->integrate_max = 0.0f;
    pid->integrate_separation_err = 0.0f;

    pid->control_output = 0.0f;
    pid->last_control_output = 0.0f;
    pid->control_output_limit = 0.0f;

    pid->dis_err = 0.0f;
    pid->dis_err_lpf = 0.0f;
    pid->last_derivative = 0.0f;
    pid->d_lpf_alpha = 0.0f;

    memset(pid->dis_error_history, 0, sizeof(pid->dis_error_history));
    memset(&pid->lpf_buf, 0, sizeof(pid->lpf_buf));
    memset(&pid->lpf_param, 0, sizeof(pid->lpf_param));
}

void PID_Init_Outer(PID_t *pid, float kp, float ki, float kd,
                    float err_max, float integ_max, float out_limit)
{
    PID_Init_Base(pid);
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->err_max = err_max;
    pid->integrate_max = integ_max;
    pid->control_output_limit = out_limit;
}

void PID_Init_Inner(PID_t *pid, float kp, float ki, float kd,
                    float err_max, float integ_max, float out_limit,
                    float sample_freq, float cutoff_freq)
{
    PID_Init_Base(pid);
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->err_max = err_max;
    pid->integrate_max = integ_max;
    pid->control_output_limit = out_limit;

    /* 巴特沃斯2阶低通滤波器设计 (用于D项) */
    set_cutoff_frequency(sample_freq, cutoff_freq, &pid->lpf_param);

    /* D项一阶低通系数 (备用, pid_ctrl_div_lpf模式不使用) */
    set_d_lpf_alpha(40, 1.0f / sample_freq, &pid->d_lpf_alpha);
}

/* ===== 外环: 简单PID (等效官方pid_ctrl_general) ===== */
/* 外环为纯P控制(kp=5.0, ki=0, kd=0), 输出=期望角速度 */
float PID_Update_Outer(PID_t *pid, float expect, float feedback, float dt)
{
    pid->expect = expect;
    pid->feedback = feedback;

    /* 偏差计算 */
    pid->last_err = pid->err;
    pid->err = pid->expect - pid->feedback;
    pid->dis_err = pid->err - pid->last_err;

    /* 偏差限幅 */
    if (pid->err_limit_flag)
        pid->err = constrain_float(pid->err, -pid->err_max, pid->err_max);

    /* 积分计算 */
    if (pid->integrate_separation_flag) {
        if (FastAbs(pid->err) <= pid->integrate_separation_err ||
            pid->integrate * pid->err < 0.0f) {
            /* 大误差时禁止继续向同方向积累，但允许反向误差卸载已有积分。 */
            pid->integrate += pid->ki * pid->err * dt;
        }
    } else {
        pid->integrate += pid->ki * pid->err * dt;
    }

    /* 积分限幅 */
    if (pid->integrate_limit_flag)
        pid->integrate = constrain_float(pid->integrate, -pid->integrate_max, pid->integrate_max);

    /* 总输出 */
    pid->last_control_output = pid->control_output;
    pid->control_output = pid->kp * pid->err
                        + pid->integrate
                        + pid->kd * pid->dis_err;

    /* 输出限幅 */
    pid->control_output = constrain_float(pid->control_output,
                                          -pid->control_output_limit,
                                          pid->control_output_limit);
    return pid->control_output;
}

/* ===== 内环: PID+巴特沃斯D项滤波 (等效官方pid_ctrl_div_lpf) ===== */
/* D项 = kd * 巴特沃斯滤波后的误差微分, 误差微分不除以dt(按采样周期) */
float PID_Update_Inner(PID_t *pid, float expect, float feedback, float dt)
{
    float integrate_before_update;
    float unsaturated_output;

    pid->expect = expect;
    pid->feedback = feedback;

    /* 偏差计算 */
    pid->pre_last_err = pid->last_err;
    pid->last_err = pid->err;
    pid->err = pid->expect - pid->feedback;
    pid->dis_err = pid->err - pid->last_err;   /* 误差微分(每采样周期) */

    /* 巴特沃斯2阶低通滤波 */
    uint16_t i;
    for (i = 4; i > 0; i--)
        pid->dis_error_history[i] = pid->dis_error_history[i - 1];
    pid->dis_error_history[0] = butterworth(pid->dis_err, &pid->lpf_buf, &pid->lpf_param);
    pid->dis_err_lpf = pid->dis_error_history[0];

    /* 偏差限幅 */
    if (pid->err_limit_flag)
        pid->err = constrain_float(pid->err, -pid->err_max, pid->err_max);

    /* 积分计算 */
    integrate_before_update = pid->integrate;
    if (pid->integrate_separation_flag) {
        if (FastAbs(pid->err) <= pid->integrate_separation_err ||
            pid->integrate * pid->err < 0.0f) {
            /* 大误差只允许反向卸载已有积分，禁止继续向饱和方向累积。 */
            pid->integrate += pid->ki * pid->err * dt;
        }
    } else {
        pid->integrate += pid->ki * pid->err * dt;
    }

    /* 积分限幅 */
    if (pid->integrate_limit_flag)
        pid->integrate = constrain_float(pid->integrate, -pid->integrate_max, pid->integrate_max);

    /* 总输出: P + I + D(巴特沃斯滤波后, 限幅±25) */
    pid->last_control_output = pid->control_output;
    unsaturated_output = pid->kp * pid->err + pid->integrate;
    unsaturated_output += pid->kd * constrain_float(pid->dis_err_lpf, -25.0f, 25.0f);

    /* 抗积分饱和：输出已到极限且误差还在把它推向同一方向时，
     * 撤销本周期积分，避免回正后积分继续把机体推过平衡点。 */
    if (FastAbs(unsaturated_output) > pid->control_output_limit &&
        unsaturated_output * pid->err > 0.0f) {
        pid->integrate = integrate_before_update;
        unsaturated_output = pid->kp * pid->err + pid->integrate;
        unsaturated_output += pid->kd * constrain_float(pid->dis_err_lpf, -25.0f, 25.0f);
    }
    pid->control_output = unsaturated_output;

    /* 输出限幅 */
    pid->control_output = constrain_float(pid->control_output,
                                          -pid->control_output_limit,
                                          pid->control_output_limit);
    return pid->control_output;
}

void PID_Reset(PID_t *pid)
{
    pid->err = 0.0f;
    pid->last_err = 0.0f;
    pid->pre_last_err = 0.0f;
    pid->integrate = 0.0f;
    pid->control_output = 0.0f;
    pid->last_control_output = 0.0f;
    pid->dis_err = 0.0f;
    pid->dis_err_lpf = 0.0f;
    pid->last_derivative = 0.0f;
    memset(pid->dis_error_history, 0, sizeof(pid->dis_error_history));
    memset(&pid->lpf_buf, 0, sizeof(pid->lpf_buf));
}

/* ===== Mixer ===== */

/* 仿照官方的姿态优先混控：
 * 1. 先保留四电机之间的姿态差速；
 * 2. 某一路触底/触顶时整体平移油门，把另一侧剩余量利用起来；
 * 3. 只有请求的差速跨度超过全部PWM范围时才等比例缩小姿态量。
 * 旧实现逐路硬裁剪会在低油门、大倾角时损失接近一半回正力。 */
void Mixer_Update_Limited(Mixer_t *m, float lower_limit)
{
    float correction[4];
    float output[4];
    float corr_min, corr_max, out_min, out_max;
    float available = MOTOR_MAX - lower_limit;
    float span, scale = 1.0f, shift = 0.0f;
    uint8_t i;

    correction[0] = -m->pitch_out + m->roll_out - m->yaw_out;
    correction[1] = -m->pitch_out - m->roll_out + m->yaw_out;
    correction[2] =  m->pitch_out - m->roll_out + m->yaw_out;
    correction[3] =  m->pitch_out + m->roll_out - m->yaw_out;

    corr_min = corr_max = correction[0];
    for (i = 1; i < 4; i++) {
        if (correction[i] < corr_min) corr_min = correction[i];
        if (correction[i] > corr_max) corr_max = correction[i];
    }
    span = corr_max - corr_min;
    if (span > available && span > 0.0f) {
        scale = available / span;
    }

    for (i = 0; i < 4; i++) {
        output[i] = m->throttle + correction[i] * scale;
    }
    out_min = out_max = output[0];
    for (i = 1; i < 4; i++) {
        if (output[i] < out_min) out_min = output[i];
        if (output[i] > out_max) out_max = output[i];
    }

    if (out_min < lower_limit) {
        shift = lower_limit - out_min;
    } else if (out_max > MOTOR_MAX) {
        shift = MOTOR_MAX - out_max;
    }

    m->m1 = constrain_float(output[0] + shift, lower_limit, MOTOR_MAX);
    m->m2 = constrain_float(output[1] + shift, lower_limit, MOTOR_MAX);
    m->m3 = constrain_float(output[2] + shift, lower_limit, MOTOR_MAX);
    m->m4 = constrain_float(output[3] + shift, lower_limit, MOTOR_MAX);
}

void Mixer_Update(Mixer_t *m)
{
    Mixer_Update_Limited(m, MOTOR_MIN);
}

void Mixer_Output(Mixer_t *m)
{
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, (uint16_t)m->m1);  /* PA8  = 左前 */
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_4, (uint16_t)m->m2);  /* PE14 = 右前 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint16_t)m->m3);  /* PA5  = 右后 */
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, (uint16_t)m->m4);  /* PA1  = 左后 */
}
