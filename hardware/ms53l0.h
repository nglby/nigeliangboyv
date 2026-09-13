#ifndef __MS53L0_H
#define __MS53L0_H

#include "i2c.h"   /* hi2c2 */

#define MS53L0_ADDR      0x52    /* HAL 8-bit I2C address (7-bit: 0x29) */

uint8_t  MS53L0_Init(void);      /* return 0=OK, 1=fail */
uint16_t MS53L0_ReadMM(void);    /* return distance in mm, 0xFFFF=timeout */

extern uint8_t ms53l0_ready;     /* 1 = init OK, safe to call ReadMM */

#endif
