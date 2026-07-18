/*
name : 你哥梁博宇
*/
#include "pid.h"
void PID_Init(PID_TypeDef *pid, float kp, float ki, float kd,
              float integral_limit, float output_limit)
{
    pid->Kp = kp;
    pid->Ki = ki;
    pid->Kd = kd;
    pid->integral_limit = integral_limit;
    pid->output_limit  = output_limit;

    pid->target     = 0;
    pid->feedback   = 0;
    pid->error      = 0;
    pid->last_error = 0;
    pid->integral   = 0;
    pid->output     = 0;
}

float PID_Calculate(PID_TypeDef *pid, float target, float feedback)
{
    pid->target   = target;
    pid->feedback = feedback;
    pid->error    = target - feedback;

    float p_out = pid->Kp * pid->error;

    pid->integral += pid->error;
   
    if(pid->integral >  pid->integral_limit) pid->integral =  pid->integral_limit;
    if(pid->integral < -pid->integral_limit) pid->integral = -pid->integral_limit;
    float i_out = pid->Ki * pid->integral;

    float d_out = pid->Kd * (pid->error - pid->last_error);
    pid->last_error = pid->error;

    pid->output = p_out + i_out + d_out;
    if(pid->output >  pid->output_limit) pid->output =  pid->output_limit;
    if(pid->output < -pid->output_limit) pid->output = -pid->output_limit;

    return pid->output;
}
