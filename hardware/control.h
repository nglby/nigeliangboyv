#ifndef CONTROL_H
#define CONTROL_H

#include "main.h"
#include "pid.h"

#define CONTROL_REQUIRE_OK    1U
#define CMD_TILT_ANGLE      5.0f  /* 前后左右最大目标倾角，降低平移速度和冲击 */
#define CMD_YAW_RATE       50.0f  
#define CMD_TIMEOUT_MS      500U  /* 方向命令需持续刷新；超时自动回到水平 */

typedef enum {
    CTRL_HOVER = 0,    
    CTRL_FORWARD,      
    CTRL_BACKWARD,     
    CTRL_LEFT,         
    CTRL_RIGHT,        
    CTRL_YAW_LEFT,      
    CTRL_YAW_RIGHT,    
} CtrlMode_t;


typedef struct {
    CtrlMode_t mode;
    float pitch_target;
    float roll_target;
    float yaw_rate_target;
    uint32_t last_command_ms;
    uint8_t command_active;
    uint8_t armed;
} Control_t;

extern volatile uint32_t control_uart_error_count;
extern volatile uint32_t control_failsafe_count;


void Control_Init(Control_t *c);

void Control_SetMode(Control_t *c, CtrlMode_t mode);

void Control_ParseChar(Control_t *c, char ch);

/* 校准和控制器初始化完成后再开启USART1接收，避免启动期间残留命令生效。 */
void Control_UART_Start(void);

/* 由USART1_IRQHandler调用，只保存最新有效命令，不在中断中printf。 */
void Control_UART_IRQHandler(void);

void Control_ProcessUART(Control_t *c);

uint8_t Control_IsArmed(const Control_t *c);

#endif
