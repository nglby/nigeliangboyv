#include "control.h"
#include "usart.h"
#include <stdio.h>
extern volatile uint8_t  alt_hold_enabled;
extern volatile float    height_target_mm;
static const char *mode_name[] = {
    "HOVER", "FWD", "BACK", "LEFT", "RIGHT",
    "YAW_L_DISABLED", "YAW_R_DISABLED"
};

static volatile char uart1_pending_char = 0;
static volatile uint8_t uart1_command_pending = 0;
static volatile uint32_t uart1_pending_tick = 0;
static volatile uint8_t uart1_armed = 0;
static volatile uint8_t uart1_arm_request = 0;
static volatile uint8_t uart1_arm_o_seen = 0;
static volatile uint32_t uart1_arm_o_tick = 0;
volatile uint32_t control_uart_error_count = 0;
volatile uint32_t control_failsafe_count = 0;

static uint8_t Control_IsCommandChar(char ch)
{
    switch (ch) {
    case 'w': case 'W':
    case 's': case 'S':
    case 'a': case 'A':
    case 'd': case 'D':
    case 'q': case 'Q':
    case 'e': case 'E':
    case 'z': case 'Z':
		case 't': case 'T':
		case 'r': case 'R':
		case 'f': case 'F':
        return 1U;
    default:
        return 0U;
    }
}

void Control_Init(Control_t *c)
{
    c->mode            = CTRL_HOVER;
    c->pitch_target    = 0.0f;
    c->roll_target     = 0.0f;
    c->yaw_rate_target = 0.0f;
    c->last_command_ms = HAL_GetTick();
    c->command_active  = 0U;
    c->armed           = CONTROL_REQUIRE_OK ? 0U : 1U;
}

void Control_SetMode(Control_t *c, CtrlMode_t mode)
{
    uint8_t mode_changed;

    if (mode > CTRL_YAW_RIGHT) {
        mode = CTRL_HOVER;
    }
    mode_changed = (c->mode != mode);
    c->mode = mode;

    switch (mode) {
    case CTRL_HOVER:
        c->pitch_target    = 0.0f;
        c->roll_target     = 0.0f;
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_FORWARD:
      
        c->pitch_target    = CMD_TILT_ANGLE;
        c->roll_target     = 0.0f;
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_BACKWARD:
        c->pitch_target    = -CMD_TILT_ANGLE;
        c->roll_target     = 0.0f;
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_LEFT:
        c->pitch_target    = 0.0f;
        c->roll_target     = -CMD_TILT_ANGLE;  
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_RIGHT:
        c->pitch_target    = 0.0f;
        c->roll_target     = CMD_TILT_ANGLE;   
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_YAW_LEFT:
        /* 当前工程没有偏航角速度PID，不能把未经闭环验证的偏航量送入混控。 */
        c->pitch_target    = 0.0f;
        c->roll_target     = 0.0f;
        c->yaw_rate_target = 0.0f;
        break;

    case CTRL_YAW_RIGHT:
        c->pitch_target    = 0.0f;
        c->roll_target     = 0.0f;
        c->yaw_rate_target = 0.0f;
        break;
    }

    /* 按键自动重复只用于刷新失联计时，不重复打印阻塞串口。 */
    if (mode_changed) {
        printf("CMD: %s pt=%.1f rt=%.1f yr=%.1f\r\n",
               mode_name[mode], c->pitch_target, c->roll_target,
               c->yaw_rate_target);
    }
}

void Control_ParseChar(Control_t *c, char ch)
{
    switch (ch) {
    case 'w': case 'W': Control_SetMode(c, CTRL_FORWARD);   break;
    case 's': case 'S': Control_SetMode(c, CTRL_BACKWARD);  break;
    case 'a': case 'A': Control_SetMode(c, CTRL_YAW_LEFT);  break;
    case 'd': case 'D': Control_SetMode(c, CTRL_YAW_RIGHT); break;
    case 'q': case 'Q': Control_SetMode(c, CTRL_LEFT);       break;
    case 'e': case 'E': Control_SetMode(c, CTRL_RIGHT);     break;
    case 'z': case 'Z': Control_SetMode(c, CTRL_HOVER);     break;
		case 't': case 'T': alt_hold_enabled = !alt_hold_enabled;printf("ALT HOLD: %s, target=%dmm\r\n",alt_hold_enabled ? "ON" : "OFF", (int)height_target_mm);break;
		case 'r': case 'R':height_target_mm += 50.0f;if (height_target_mm > 2000.0f) height_target_mm = 2000.0f;printf("ALT TARGET: %dmm\r\n", (int)height_target_mm);break;
		case 'f': case 'F':height_target_mm -= 50.0f;if (height_target_mm < 100.0f) height_target_mm = 100.0f;printf("ALT TARGET: %dmm\r\n", (int)height_target_mm);break;
    default: break;
    }
}

void Control_UART_Start(void)
{
    uint32_t primask = __get_PRIMASK();
    volatile uint32_t dummy;

    __disable_irq();
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
    dummy = huart1.Instance->SR;
    dummy = huart1.Instance->DR;
    (void)dummy;
    uart1_pending_char = 0;
    uart1_command_pending = 0U;
    uart1_pending_tick = HAL_GetTick();
    uart1_armed = CONTROL_REQUIRE_OK ? 0U : 1U;
    uart1_arm_request = 0U;
    uart1_arm_o_seen = 0U;
    uart1_arm_o_tick = 0U;
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_RXNE);
    if (primask == 0U) {
        __enable_irq();
    }
}

void Control_UART_IRQHandler(void)
{
    uint32_t status = huart1.Instance->SR;

    if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_NE | USART_SR_FE)) != 0U) {
        char ch = (char)(huart1.Instance->DR & 0xFFU);

        /* 先读SR再读DR可同时清除ORE/NE/FE。错误字节不执行。 */
        if ((status & (USART_SR_ORE | USART_SR_NE | USART_SR_FE)) != 0U) {
            control_uart_error_count++;
            return;
        }
        if (!uart1_armed) {
            if (ch == 'o' || ch == 'O') {
                uart1_arm_o_seen = 1U;
                uart1_arm_o_tick = HAL_GetTick();
            } else if ((ch == 'k' || ch == 'K') && uart1_arm_o_seen &&
                       ((uint32_t)(HAL_GetTick() - uart1_arm_o_tick) <= 1000U)) {
                uart1_arm_o_seen = 0U;
                uart1_armed = 1U;
                uart1_arm_request = 1U;
            } else {
                uart1_arm_o_seen = 0U;
            }
            return;
        }
        if (((status & USART_SR_RXNE) != 0U) && Control_IsCommandChar(ch)) {
            /* 邮箱保留最新有效命令；Z不会因前面字符堆积而被延迟。 */
            uart1_pending_char = ch;
            uart1_pending_tick = HAL_GetTick();
            uart1_command_pending = 1U;
        }
    }
}

void Control_ProcessUART(Control_t *c)
{
    char ch = 0;
    uint32_t command_tick = 0;
    uint8_t have_command = 0U;
    uint8_t have_arm_request = 0U;
    uint32_t primask = __get_PRIMASK();

    /* 与USART1中断交换一个最新命令，临界区只有几个寄存器操作。 */
    __disable_irq();
    if (uart1_arm_request) {
        uart1_arm_request = 0U;
        have_arm_request = 1U;
    }
    if (uart1_command_pending) {
        ch = uart1_pending_char;
        command_tick = uart1_pending_tick;
        uart1_command_pending = 0U;
        have_command = 1U;
    }
    if (primask == 0U) {
        __enable_irq();
    }

    if (have_arm_request) {
        c->armed = 1U;
        c->command_active = 0U;
        c->last_command_ms = HAL_GetTick();
        Control_SetMode(c, CTRL_HOVER);
        printf("ARMED: OK accepted, motor ramp starting\r\n");
        return;
    }

    if (!c->armed) return;

    if (have_command) {
        Control_ParseChar(c, ch);
        c->last_command_ms = command_tick;
        c->command_active = (c->mode == CTRL_FORWARD ||
                             c->mode == CTRL_BACKWARD ||
                             c->mode == CTRL_LEFT ||
                             c->mode == CTRL_RIGHT) ? 1U : 0U;
    }

    if (c->command_active &&
        (uint32_t)(HAL_GetTick() - c->last_command_ms) > CMD_TIMEOUT_MS) {
        Control_SetMode(c, CTRL_HOVER);
        c->command_active = 0U;
        control_failsafe_count++;
        printf("RC FAILSAFE: command timeout, target returned to level\r\n");
    }
}

uint8_t Control_IsArmed(const Control_t *c)
{
    return c->armed;
}
