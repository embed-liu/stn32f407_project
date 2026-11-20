#ifndef __I2C_H
#define __I2C_H
#include "stm32f4xx.h"
#include "stm32f4xx_rcc.h"
#include "stm32f4xx_gpio.h"
#include "system.h"
#include "FreeRTOS.h"
#include "task.h"

// 临界区包裹 I2C 时序，防止FreeRTOS打断时序
#define I2C_ENTER_CRITICAL()  taskENTER_CRITICAL()
#define I2C_EXIT_CRITICAL()   taskEXIT_CRITICAL()

#define IIC_SCL    PBout(8) //SCL
#define IIC_SDA    PBout(9) //SDA
#define READ_SDA   PBin(9)  //输入SDA

void IIC_Init(void);
void IIC_Start(void);
void IIC_Stop(void);
void IIC_Send_Byte(uint8_t txd);
uint8_t IIC_Read_Byte(uint8_t ack);
uint8_t IIC_Wait_Ack(void);
void IIC_Ack(void);
void IIC_NAck(void);

#endif
